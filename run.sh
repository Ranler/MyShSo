#!/bin/sh

BASEDIR=`dirname $0`

make_lib ()
{
    cd $BASEDIR/lib

    case $1 in
	"make")
	    make -f makefile
	    ;;
	"clean")
	    make -f makefile clean
    esac

    cd -
}

make_app ()
{
    cd $BASEDIR/src

    case $1 in
	"make")
	    make -f mp.make
	    make -f mt.make
	    ;;
	"clean")
	    make -f mp.make clean
	    make -f mt.make clean
    esac
    
    cd -
}

run_test ()
{
    cd $BASEDIR/test
    sh run_test.sh
    cd -
}


CMD=$1
case $CMD in
    "make"|"clean")
	make_lib $CMD
	make_app $CMD
	;;
    "test")
	run_test
	;;
    *)
	echo "Error Arguments!"
	exit 1
esac

