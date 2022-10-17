num=$1
mode=$2
cp unix_redis.conf /home/redis-4.0.2/src/
cd /home/redis-4.0.2/src/

for ((i=0; i<$num; i++)); do
	port=$(($i + 379))
    dir=6${port}
	mkdir -p redis-$dir
    if [ "$mode" == "tcp" ]; then
        cp tcp_redis.conf redis-$dir/
        cd redis-$dir/
        sed -i 's/6379/'$dir'/g' tcp_redis.conf
    elif [ "$mode" == "sock" ]; then
        cp unix_redis.conf redis-$dir/
        cd redis-$dir/
        sed -i 's/6379/'$dir'/g' unix_redis.conf
    else
        echo "Warning, Unknown mode option."
    fi
    cd ../
    cp redis-server redis-$dir/
    echo "id is $dir"
done

