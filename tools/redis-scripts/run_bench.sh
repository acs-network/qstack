num=$1

com=$2

cmd=$3

cd /home/redis-4.0.2/src/

#echo never > /sys/kernel/mm/transparent_hugepage/enabled

for ((i=0; i<$num; i++)); do
    if [ "$com" == "up" ]; then
        port=$(($i + 79))
        dir=63${port}
		core=$(($i + 0))
        echo "id is $dir"
        #taskset -c $core ./redis-$dir/redis-benchmark -p $dir -t $cmd -r 2000000 -n 20000000 &
        taskset -c $core ./redis-$dir/redis-benchmark -s /run/redis/redis_$dir.sock -t $cmd -r 2000000 -n 20000000 &
    elif [ "$com" == "down" ]; then
        killall -9 redis-benchmark
    fi
done

