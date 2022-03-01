num=$1
cd /home/redis-4.0.2/src/

for ((i=0; i<$num; i++)); do
	port=$(($i + 79))
    dir=63${port}
	mkdir -p redis-$dir
    cp redis-benchmark redis-$dir/
    cd redis-$dir/
    echo "id is $dir"
    cd ../
done

