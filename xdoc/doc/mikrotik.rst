.. image:: /png/clownix64.png 
   :align: right

=======================
Mikrotik use in cloonix
=======================

In directory tools/mikrotik you can find scripts to help in building a
mikrotik so as to test the mikrotik support from cloonix.
  
The result after the use of these scripts gives the demonstration that
you can see on the picture below.
  
.. image:: /png/mikro.png
  
  
The 2 scripts in tools/mikrotik are:
  
    * *step1_make_mikrotik.sh* gets an image from mikrotik and creates a qcow2.
    * *step2_run_demo.sh* This runs the demo and gives the pictures above.

Notice that the mikrotik is red, this is because the backdoor that
makes the cloonix_ssh and cloonix_scp possible does not exist in an
non-linux or non-rootable environment, as a convention of color, when
cloonix server can send and receive messages from the cloonix agent within the
guest vm, the guest vm gets the blue color.
For this no backdoor cloonix vm, cloonix option --nobackdoor is used.
  
Notice also that the mikrotik has a nat attached to its interface eth0.
This nat permits to have the equivalent of cloonix_ssh and cloonix_scp
through an openssh client. The mikrotik must have dhcp configured on the
interface 0, the nat provides an ip address. To get an admin shell, do::
  
    *cloonix_osh nemo mikro*
  
You have another way of getting an admin shell, just double-click when mouse
is above the mikro vm.
  
The same way, a file can be uploaded or downloaded, by doing::
  
    *cloonix_ocp nemo <file> mikro:<dir>*
  
For the nat and the associated cloonix_osh and cloonix_ocp functions,
cloonix option --natplug=<num> is used, num is the interface number.
  

Mikrotik nat
------------

  Here is the logical pathway for the cloonix_osh function.
  
::

      +---------------+
      | cloonix osh   |
      +---------------+
      | openssh client| patched openssh to enter cloonix multiplexed socket
      +---+-----------+
          |
          | SSH over TCP multiplexed in tcp socket from client to server cloonix
          |
      +---+-----------+
      | server doorway|
      +---+-----------+
          | 
          | SSH over TCP in inter server process unix socket
          |
      +---+-------------+
      | process dpdk nat|
      +---+-------------+
          |
          | RAW TCP (syn synack ...) over dpdk support 
          |
       +--+-------+
       |   mikro  | 
       +----------+
  

