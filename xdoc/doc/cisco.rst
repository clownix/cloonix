.. image:: /png/clownix64.png 
   :align: right

====================
Cisco use in cloonix
====================

In directory tools/cisco you can find scripts to help in building a cisco
c8000v so as to test the cisco support from cloonix. 

The result after the use of these scripts gives the demonstration that
you can see on the picture below.

.. image:: /png/cisco.png


The 3 scripts in tools/cisco are:

  * *step1_make_preconf_iso.sh* create an iso with a cisco configuration.
  * *step2_make_qcow2.sh*  creates the cisco qcow2.
  * *step3_run_demo.sh*  This runs the demo and gives the picture above.

Notice that the 3 ciscos are red, this is because the backdoor that
makes the cloonix_ssh and cloonix_scp possible does not exist in an
alien environment such as cisco, as a convention of color, when cloonix
server can send and receive messages from the cloonix agent within the
guest vm, the guest vm gets the blue color.
For this no backdoor cloonix vm, cloonix option *--nobackdoor* is used.

Notice also that the ciscos each have a nat attached to the interface
eth0, this nat permits to have the equivalent of cloonix_ssh and cloonix_scp
through an openssh client. The cisco must have dhcp configured on the
interface 0. cloonix option *--natplug=0* creates this nat plugged to eth0.
To get an admin terminal on cisco, do::

  *cloonix_osh nemo cisco1*

You have another way of getting an admin shell, just double-click when mouse
is above the cisco1.

The same way, a file can be uploaded to cisco or downloaded from cisco 
by doing::

  *cloonix_ocp nemo run_configs/cisco1.cfg cisco1:running-config*

The above command sends the cisco1.cfg into the cisco1 vm, the setup
of a demo is easy using these tools.

For the building of the cisco, you have to get a file such as:
  
  *c8000v-universalk9.17.04.01a.iso*

The script will probably still work with another iso from cisco.
  

Cisco nat
---------

  Here is the logical pathway for the cloonix_osh function, it utilises
  the nat cloonix object to work.
  
::

 C  {   +---------------+
 L  {   | cloonix osh   |
 I  {   +---------------+
 E  {   | openssh client| patched openssh enters cloonix socket to server
 N  {   +---+-----------+
 T         |
           | SSH over TCP multiplexed in tcp socket from client to server
           |
    {  +---+-----------+
 S  {  | server doorway|
    {  +---+-----------+
 E  {      | 
    {      | SSH over TCP in inter server process unix socket
 R  {      |
    {  +---+-------------+
 V  {  | process dpdk nat|
    {  +---+-------------+
 E  {      |
    {      | RAW TCP (syn synack ...) over dpdk support 
 R  {      |
    {   +--+-------+
    {   |   cisco  | 
    {   +----------+
  

