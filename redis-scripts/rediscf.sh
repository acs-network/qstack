sed -i '$a\vm.overcommit_memory = 1' /etc/sysctl.conf
sed -i '$a\net.core.somaxconn = 2048' /etc/sysctl.conf
sed -i 'vm.swappiness={bestvalue}' /etc/sysctl.conf
sysctl -p
sed -i '$a\echo never > /sys/kernel/mm/transparent_hugepage/enabled' /etc/rc.local
chmod +x /etc/rc.local
echo never > /sys/kernel/mm/transparent_hugepage/enabled
