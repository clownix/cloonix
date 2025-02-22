#!/bin/bash
#----------------------------------------------------------------------
PARAMS="ram=1000 cpu=2 eth=s"
#----------------------------------------------------------------------
cloonix_net nemo
#----------------------------------------------------------------------
cloonix_gui nemo
#----------------------------------------------------------------------
cloonix_cli nemo add kvm iptb ram=1000 cpu=2 eth=sss bookworm.qcow2 &
for i in 0 1 2; do
  cloonix_cli nemo add kvm kvm${i} ram=1000 cpu=2 eth=s bookworm.qcow2 &
  cloonix_cli nemo add lan kvm${i} 0 lan${i}
  cloonix_cli nemo add lan iptb ${i} lan${i}
done
#----------------------------------------------------------------------
set +e
for i in kvm0 kvm1 kvm2 iptb ; do
  while ! cloonix_ssh nemo ${i} "echo" 2>/dev/null; do
    echo ${i} not ready yet, waiting 1 sec
    sleep 1
  done
done
set -e
#----------------------------------------------------------------------
cloonix_ssh nemo kvm0 "ip addr add dev eth0 192.168.0.1/24"
cloonix_ssh nemo kvm0 "ip addr add dev eth0 172.16.0.1/24"





ovs-vsctl set port vnet0 tag=10
ip netns add red
ip link add veth0 type veth peer name veth1
ip link set veth1 netns red
ip netns exec red ip addr add 10.1.2.1/24 dev veth1
ip netns exec red ip link set veth1 up
ovs-vsctl add-port br-int veth0
ovs-vsctl set port veth0 tag=20

ovs-vsctl set port vnet0 tag=20
ip netns add blue
ip link add veth0 type veth peer name veth1
ip link set veth1 netns blue
ip netns exec blue ip addr add 10.1.1.1/24 dev veth1
ip netns exec blue ip link set veth1 up
ovs-vsctl add-port br-int veth0
ovs-vsctl set port veth0 tag=10

ovs-vsctl set port gre0 trunks=10,20,30






sudo ip netns add ns1
sudo ip netns add ns2
sudo ip netns add ns3
sudo ip netns add ns4

sudo ip link add veth1 type veth peer name veth11
sudo ip link add veth2 type veth peer name veth12
sudo ip link add veth3 type veth peer name veth13
sudo ip link add veth4 type veth peer name veth14

sudo ip link set veth11 netns ns1
sudo ip link set veth12 netns ns2
sudo ip link set veth13 netns ns3
sudo ip link set veth14 netns ns4

sudo ip netns exec ns1  ifconfig lo up
sudo ip netns exec ns2  ifconfig lo up
sudo ip netns exec ns3  ifconfig lo up
sudo ip netns exec ns4  ifconfig lo up

sudo ifconfig veth1 10.1.11.1/24 up
sudo ifconfig veth2 10.1.12.1/24 up
sudo ifconfig veth3 10.1.13.1/24 up
sudo ifconfig veth4 10.1.14.1/24 up

sudo ip netns exec ns1 ifconfig veth11 10.1.11.2/24 up
sudo ip netns exec ns2 ifconfig veth12 10.1.12.2/24 up
sudo ip netns exec ns3 ifconfig veth13 10.1.13.2/24 up
sudo ip netns exec ns4 ifconfig veth14 10.1.14.2/24 up

sudo ip netns exec ns1 route add default gw 10.1.11.1 veth11
sudo ip netns exec ns2 route add default gw 10.1.12.1 veth12
sudo ip netns exec ns3 route add default gw 10.1.13.1 veth13
sudo ip netns exec ns4 route add default gw 10.1.14.1 veth14
#----------------------------------------------------------------------
sudo vconfig add veth1 11
sudo vconfig add veth3 11
sudo vconfig add veth11 12
sudo vconfig add veth13 12
