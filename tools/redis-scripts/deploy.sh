nodes=$1
cmd=$2
filename="$3"
com=$4

set_lnk()
{
	id=0
    for line in `cat $filename`
    do
		ssh root@$line 'ifconfig enp49s0f1 down && mkdir -p /home/songhui/qingyun/dpdk-lnk && cd /home/songhui/qingyun/dpdk-lnk && ln -s ../dpdk/x86_64-native-linuxapp-gcc/include incude && ln -s ../dpdk/x86_64-native-linuxapp-gcc/lib lib' &
        id=$(($id+1))
    	if [ $id -eq $nodes ]
    	then
        	break;
    	fi
	done
}

set_qingyun()
{
	id=0
    for line in `cat $filename`
    do
		echo "Node $id, setting ip"
    	tmp=${line%.*}
    	ip1=${tmp##*.}
    	ip2=${line##*.}
		ssh root@$line 'cd /home/songhui/qingyun/ && ./setup_mtcp_dpdk_env.sh < dpdk-inputfile && ifconfig dpdk0 192.168.'$ip2'.100/16 up' &
        id=$(($id+1))
    	if [ $id -eq $nodes ]
    	then
        	break;
    	fi
	done
}

setdir_redis()
{
	num=$nodes
    id=0

    for line in `cat $filename`
    do
		echo "Node $id, setting redis"
		ssh root@$line 'bash -s' $num < dir-redis.sh &
        id=$(($id+1))
    	if [ $id -eq $nodes ]
    	then
        	break;
    	fi
    done
}

start_redis()
{
	num=$nodes
    id=0

    for line in `cat $filename`
    do
		echo "Node $id, $com redis"
		ssh root@$line 'bash -s' $num $com < setup-redis.sh &
        id=$(($id+1))
    	if [ $id -eq $nodes ]
    	then
        	break;
    	fi
    done
}

set_sysconf()
{
	num=$nodes
    id=0

    for line in `cat $filename`
    do
		echo "Node $id, $com redis"
		ssh root@$line 'bash -s' < rediscf.sh &
        id=$(($id+1))
    	if [ $id -eq $nodes ]
    	then
        	break;
    	fi
    done
}

scp()
{	
	./scp_file.sh /home/songhui/qingyun /home/songhui/ $filename
	./scp_file.sh redis.conf /home/songhui/qingyun/redis-4.0.2/src/ $filename
}

if [ "$cmd" == "help" ]; then
    echo "./deploy.sh <command> <number of nodes> <hosts file>"
    echo "command:"
    echo "lnk       : set dpdk soft link"
    echo "qyun   	: deploy qingyun" 
    echo "redir     : set redis path and configuration"
    echo "redis   	: start/kill redis process" 
    echo "scp   	: scp redis files to multi hosts" 
elif [ "$cmd" == "lnk" ]; then
  set_lnk
elif [ "$cmd" == "qyun" ]; then
  set_qingyun
elif [ "$cmd" == "redir" ]; then
  setdir_redis
elif [ "$cmd" == "syscf" ]; then
  set_sysconf
elif [ "$cmd" == "redis" ]; then
  start_redis
elif [ "$cmd" == "scp" ]; then
  scp
else
  echo "Warning, Unknown option."
fi
