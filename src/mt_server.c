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
#include <pthread.h>

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
        pthread_exit((void*)1);
    }
    pthread_exit((void*)0);
}

/* return:
 *     real send size (must be equal with parameter `len`) 
 */ 
inline static size_t sendall(int fd, void *buf, size_t len, int flags)
{
    size_t ready_len = 0;
    while (ready_len < len) {
        int size = send(fd, (char*)buf+ready_len, len-ready_len, flags);
	if (size < 0) break;
	ready_len += (size_t)size;
    }
    return ready_len;
}

inline static int crypt_recv(int fd, void *buf, size_t len)
{
    int size = recv(fd, buf, len, 0);
    if (size > 0) {
      shadow_decrypt(buf, &chd_crypto, (size_t)size);
    }
    return size;
}


inline static size_t crypt_sendall(int fd, void *buf, size_t len)
{
    shadow_encrypt(buf, &chd_crypto, len);
    return sendall(fd, buf, len, 0);
}

static int read_addr(int fd, uint8_t *buf, struct sockaddr** addr, size_t *addr_len) {
    if (crypt_recv(fd, buf, 1) <= 0) return -1;
    uint8_t addrtype = buf[0];

    switch(addrtype) {
        case ADDRTYPE_IPV4:    /* |addr(4)|+|port(2)|*/
  	    if (crypt_recv(fd, buf, 6) != 6) return -1;

	    struct sockaddr_in *addr_in
	        = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	    memset(addr_in, 0, sizeof(struct sockaddr_in));
            addr_in->sin_family = AF_INET;
            memcpy(&(addr_in->sin_addr), buf, 4);
	    memcpy(&(addr_in->sin_port), buf+4, 2);
            printf("INFO: ipv4 addr \"%s:%hu\"\n",
		   inet_ntoa(addr_in->sin_addr),
		   ntohs(addr_in->sin_port));

	    *addr = (struct sockaddr*)addr_in;
	    *addr_len = sizeof(struct sockaddr_in);
            break;

        case ADDRTYPE_DOMAIN:    /* |addr_len(1)|+|host(addr_len)|+|port(2)| */
	    if (crypt_recv(fd, buf, 1) != 1) return -1;

	    uint8_t domain_addr_len = buf[0];
            if (crypt_recv(fd, buf, domain_addr_len + 2) != (domain_addr_len + 2)) return -1;
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
            if (crypt_recv(fd, buf, 18) != 18) return -1;

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


void *child_core(void *arg)
{
    int fd = (int)arg;

    /* make child encryptor*/
    make_encryptor(&crypto, &chd_crypto, 0, NULL);


    uint8_t buf[BUF_SIZE];
    size_t remote_addr_len;
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

    size_t read_size;
    int rc;
    while (poll(fds, 2, -1) > 0) {
        if (fds[0].revents & (POLLIN | POLLHUP)) {
            rc = crypt_recv(fd, buf, BUF_SIZE);
	    if (rc > 0) {
	        read_size = (size_t)rc;
		if (sendall(remote_fd, buf, read_size, 0) < read_size) break;
            }
            else if (rc == 0) {
                fds[0].fd = -1;
                shutdown(fd, SHUT_RD);
                shutdown(remote_fd, SHUT_WR);
            }
            else {
                break;
            }
        }
        
        if (fds[1].revents & (POLLIN | POLLHUP)) {
            rc = recv(remote_fd, buf, BUF_SIZE, 0);
            if (read_size > 0) {
	        read_size = (size_t)rc;
		if (crypt_sendall(fd, buf, read_size) < read_size) break;
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
    if (errno != 0) {
      clean_exit(errno, "socket pair error exit", 0);
    }
    return (void*)0; 
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

    pthread_t child_t;
    for(;;) {
        int child_fd = accept(server_fd, NULL, NULL);
	if (child_fd == -1) {
	    clean_exit(errno, "accept error", 0);
	}

	int rt = pthread_create(&child_t, NULL, child_core, (void*)child_fd);
	if (rt != 0) {
	  clean_exit(errno, "create child thread error", 2, server_fd, child_fd);
        }
    }
}

int main()
{
    make_encryptor(NULL, &crypto, METHOD_SHADOWCRYPT, (uint8_t*)PASSWORD);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PROXY_PORT);
    inet_pton(AF_INET, PROXY_IP_ADDR, &(addr.sin_addr));

    server_core((struct sockaddr*)&addr, sizeof(addr));

    return 0;
}
