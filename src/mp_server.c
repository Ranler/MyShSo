#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <poll.h>
#include <string.h>
#include <signal.h>

#include <syslog.h>

#include "server.h"
#include "encrypt.h"


struct encryptor crypto;
struct encryptor chd_crypto;


static void clean_exit(int err, const char *msg, int fds_num, ...)
{
    if (err != 0) {
        fprintf(stderr, "ERROR: %s: %s\n", msg, strerror(err));
    }
    else {
        fprintf(stdout, "INFO: %s\n", msg);
    }
    
    va_list arg_ptr;
    va_start(arg_ptr, fds_num);
    while (fds_num > 0) {
        close(va_arg(arg_ptr, int));
    }
    va_end(arg_ptr);

    destroy_encryptor(&crypto);
    destroy_encryptor(&chd_crypto);

    if (err != 0) {
        exit(EXIT_FAILURE);      
    }
    exit(EXIT_SUCCESS);
}

/* return:
 *     > 0: received size
 *     = 0: EOF
 *      -1: ERROR
 */ 
inline static int recv_buf(int fd, void *buf, size_t len)
{
    int size = recv(fd, buf, len, 0);
    if (size > 0) {
        shadow_encrypt(buf, &chd_crypto, size);
    }
    return size;
}

/* return:
 *     real send size (must be equal with parameter `len`) 
 */ 

inline static int send_buf_all(int fd, void *buf, size_t len)
{
    shadow_decrypt(buf, &chd_crypto, len);
    
    size_t real_len = 0;
    int size = send(fd, buf, len, 0);
    real_len += size;    
    while (size > 0 && real_len < len) {
        size = send(fd, (char*)buf+real_len, len-real_len, 0);
        real_len += size;        
    }
    return real_len;
}

static int read_addr(int fd, uint8_t *buf, struct sockaddr** addr, int *addr_len) {
    if (recv_buf(fd, buf, 1) <= 0) return -1;
    uint8_t addrtype = buf[0];

    switch(addrtype) {
        case ADDRTYPE_IPV4:    /* |addr(4)|+|port(2)|*/
  	    if (recv_buf(fd, buf, 6) != 6) return -1;

	    struct sockaddr_in *addr_in
	        = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	    memset(addr_in, 0, sizeof(struct sockaddr_in));
            addr_in->sin_family = AF_INET;
            memcpy(&(addr_in->sin_addr), buf, 4);
            addr_in->sin_port = ntohs(*(uint16_t*)(buf+4)); // check

            printf("INFO: ipv4 addr \"%s\"\n", inet_ntoa(addr_in->sin_addr));
	    *addr = (struct sockaddr*)addr_in;
	    *addr_len = sizeof(struct sockaddr_in);
            break;

        case ADDRTYPE_DOMAIN:    /* |addr_len(1)|+|host(addr_len)|+|port(2)| */
	    if (recv_buf(fd, buf, 1) != 1) return -1;

	    uint8_t domain_addr_len = buf[0];
            if (recv_buf(fd, buf, domain_addr_len + 2) != (domain_addr_len + 2)) return -1;
            char domain_port_str[8];
            sprintf(domain_port_str, "%hu", ntohs(*(uint16_t*)(buf+domain_addr_len)));

	    struct sockaddr_un *addr_un
	        = (struct sockaddr_un*)malloc(sizeof(struct sockaddr_un));
	    memset(addr_un, 0, sizeof(struct sockaddr_un));
	    addr_un->sun_family = AF_UNIX;
	    memcpy(addr_un->sun_path, buf, domain_addr_len);

	    /* domain name -> IP */
	    struct addrinfo *res, *rp;
	    if (getaddrinfo(addr_un->sun_path, domain_port_str, NULL, &res) != 0) return -1;
	    for (rp=res; rp!=NULL; rp=rp->ai_next) {
	        printf("INFO: domain addr \"%s(%s):%s\"\n",
                       addr_un->sun_path,
                       inet_ntoa(((struct sockaddr_in*)(rp->ai_addr))->sin_addr), 
                       domain_port_str);
                *addr = (struct sockaddr*)malloc(rp->ai_addrlen);
		memcpy(*addr, rp->ai_addr, rp->ai_addrlen);
		*addr_len = rp->ai_addrlen;
		break;
	    }

	    free(addr_un);
	    freeaddrinfo(res);
	    break;

        case ADDRTYPE_IPV6:  /* |addr(16)|+|port(2)|*/
            if (recv_buf(fd, buf, 18) != 18) return -1;

	    struct sockaddr_in6 *addr_in6
	        = (struct sockaddr_in6*)malloc(sizeof(struct sockaddr_in6));
	    memset(addr_in6, 0, sizeof(struct sockaddr_in6));
            addr_in6->sin6_family = AF_INET6;
            memcpy(&(addr_in6->sin6_addr), buf, 16);
            addr_in6->sin6_port = ntohs(*(uint16_t*)(buf+16)); // check

            printf("INFO: ipv6 addr\n");
	    *addr = (struct sockaddr*)addr_in6;
	    *addr_len = sizeof(struct sockaddr_in6);
            break;
	  
        default:
  	    printf("ERROR: addrtype: %d\n", addrtype);
	    return -1;
    }
    return 0;
}


void child_core(int fd)
{
    /* make child encryptor*/
    make_encryptor(&crypto, &chd_crypto, 0, NULL);


    uint8_t buf[BUF_SIZE];
    int remote_addr_len;
    struct sockaddr *remote_addr;
    if (read_addr(fd, buf, &remote_addr, &remote_addr_len) != 0) {
        clean_exit(errno, "get remote addr error", 1, fd);
    }

    int remote_fd;
    if ((remote_fd = socket(remote_addr->sa_family, SOCK_STREAM, 0)) < 0) {
        clean_exit(errno, "cannot open socket", 1, fd);
    }

    int optval = 1;
    if (setsockopt(remote_fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) == -1) {
        clean_exit(errno, "set socket option error", 1, fd);
    }
        
    if (connect(remote_fd, remote_addr, remote_addr_len) < 0 ) {
        clean_exit(errno, "cannnot connect remote", 1, fd);
    }
    free(remote_addr);
    remote_addr = NULL;
    remote_addr_len = 0;


    /* use poll */
    struct pollfd fds[2];
    fds[0].fd = fd;
    fds[1].fd = remote_fd;
    fds[0].events = POLLIN | POLLERR | POLLHUP;
    fds[1].events = POLLIN | POLLERR | POLLHUP;

    int read_size;
    while (poll(fds, 2, -1) > 0) {
        if (fds[0].revents & (POLLIN | POLLHUP)) {
            read_size = recv_buf(fd, buf, BUF_SIZE);
            if (read_size > 0) {
                if (send_buf_all(remote_fd, buf, read_size) < read_size) break;
            }
            else if (read_size == 0) {
                fds[0].fd = -1;
                shutdown(fd, SHUT_RD);
                shutdown(remote_fd, SHUT_WR);
            }
            else {
                break;
            }
        }
        
        if (fds[1].revents & (POLLIN | POLLHUP)) {
            read_size = recv_buf(remote_fd, buf, BUF_SIZE);
            if (read_size > 0) {
                if (send_buf_all(fd, buf, read_size) < read_size) break;
            }
            else if (read_size == 0) {
                fds[1].fd = -1;
                shutdown(fd, SHUT_WR);
                shutdown(remote_fd, SHUT_RD);              
            }
            else {
                break;
            }
        }

        if (fds[0].fd == fds[1].fd) clean_exit(0, "close all sock", 0);
        if ((fds[0].revents & POLLERR) || (fds[1].revents & POLLERR)) break;
    }
    clean_exit(errno, "socket pair error exit", 0);
}

int server_core(const struct sockaddr *addr, socklen_t alen)
{
    /* TODO Register Signal */
    signal(SIGCHLD, SIG_IGN);

    int server_fd = socket(addr->sa_family, SOCK_STREAM, 0);
    if (server_fd < 0) {
        clean_exit(errno, "cannot open socket", 0);
    }

    int sockopt_on = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &sockopt_on, sizeof(sockopt_on)) == -1) {
        clean_exit(errno, "set socket option error", 0);
    }

    if (bind(server_fd, addr, alen) < 0) {
        clean_exit(errno, "cannot bind socket", 0);
    }

    if (listen(server_fd, SOMAXCONN) < 0) {
        clean_exit(errno, "cannot listen socket", 0);
    }
   
    printf("INFO: Begin to listen %d\n", 
	   ntohs(((struct sockaddr_in*)addr)->sin_port));
    for(;;) {
        int child_fd = accept(server_fd, NULL, NULL);
	if (child_fd == -1) {
	    clean_exit(errno, "accept error", 0);
	}

	int child_pid = fork();
        if (child_pid == 0) { /* child */
            close(server_fd);
            child_core(child_fd);
        }
        else if (child_pid > 0) { /* parent */
            close(child_fd);
        }
        else { /* error */
            clean_exit(errno, "fork error", 2, server_fd, child_fd);
        }
    }
}

int main(int argc, char *argv[])
{

    uint8_t *pwd = (uint8_t*)PASSWORD;
    uint8_t crypt_method = METHOD_SHADOWCRYPT;
    make_encryptor(NULL, &crypto, crypt_method, pwd);


    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PROXY_PORT);
    inet_pton(AF_INET, PROXY_IP_ADDR, &(addr.sin_addr));

    server_core((struct sockaddr*)&addr, sizeof(addr));

    return 0;
}