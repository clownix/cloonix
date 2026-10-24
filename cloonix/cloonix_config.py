# The nets of this cloonix: doorways address, password, web port, c2c udp.

CONFIG = r"""
CLOONIX_VERSION=101
# cloonix_web: https port of the web gui (net name + cloonix_passwd), optional, headless without.

CLOONIX_NET: nemo {
  cloonix_ip       127.0.0.1
  cloonix_port     45211
  cloonix_passwd   nemoclown
  cloonix_web      4444
  c2c_udp_ip       127.0.0.1
  c2c_udp_port_low 51713
}

CLOONIX_NET: mito {
  cloonix_ip       127.0.0.1
  cloonix_port     45212
  cloonix_passwd   mitoclown
  cloonix_web      4454
  c2c_udp_ip       127.0.0.1
  c2c_udp_port_low 52713
}

CLOONIX_NET: fido {
  cloonix_ip       127.0.0.1
  cloonix_port     45213
  cloonix_passwd   fidoclown
  cloonix_web      4464
  c2c_udp_ip      127.0.0.1
  c2c_udp_port_low 53713
}

CLOONIX_NET: cucu {
  cloonix_ip       10.7.7.1
  cloonix_port     45217
  cloonix_passwd   cucuclown
  cloonix_web      4474
  c2c_udp_ip       10.7.7.1
  c2c_udp_port_low 57713
}

CLOONIX_NET: lolo {
  cloonix_ip       10.7.7.2
  cloonix_port     45218
  cloonix_passwd   loloclown
  cloonix_web      4484
  c2c_udp_ip       10.7.7.2
  c2c_udp_port_low 58713
}

CLOONIX_NET: endu {
  cloonix_ip       192.168.100.175
  cloonix_port     45219
  cloonix_passwd   enduclown
  cloonix_web      4494
  c2c_udp_ip       192.168.100.175
  c2c_udp_port_low 59713
}

"""
