num=$1

com=$2

mode=$3

cd /home/redis-4.0.2/src/

echo never > /sys/kernel/mm/transparent_hugepage/enabled
# core binding for 6130
CORES=(34 35 36 37 42 43 44 45 50 51 52 53 58 59 60 61)

for ((i=0; i<$num; i++)); do
    if [ "$com" == "up" ]; then
        port=$(($i + 379))
        dir=6${port}
#		core=$(($i + 18))
        core=${CORES[$i]}
        echo "id is $dir, core $core"
        if [ "$mode" == "tcp" ]; then
            taskset -c $core ./redis-$dir/redis-server ./redis-$dir/redis.conf &
        elif [ "$mode" == "sock" ]; then
            taskset -c $core ./redis-$dir/redis-server ./redis-$dir/unix_redis.conf &
        else
            echo "Unknown mode!"
        fi
    elif [ "$com" == "down" ]; then
        killall -9 redis-server
    fi
done

