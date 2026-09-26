/* web_pty.c — PTY shell bridge (ssh / console / container shell).
 * Part of the GUI server TU; see web_common.h. */

#include "web_common.h"

static void ws_pty_bridge(int browser_fd, int pty_master)
{
    char buf[BUF_SIZE]; fd_set rd;
    int maxfd=(browser_fd>pty_master?browser_fd:pty_master)+1;
    while(1){
        FD_ZERO(&rd);FD_SET(browser_fd,&rd);FD_SET(pty_master,&rd);
        int rc=select(maxfd,&rd,NULL,NULL,NULL);
        if(rc<0){if(errno==EINTR)continue;break;}if(rc==0)break;
        if(FD_ISSET(browser_fd,&rd)){
            uint8_t *payload=NULL;uint64_t plen=0;
            int op=ws_read_frame(browser_fd,&payload,&plen);
            if(op<0){free(payload);break;}
            if(op==8){free(payload);break;}
            if((op==1||op==2)&&payload&&plen){
                int cols=0,rows=0;
                if(op==1&&sscanf((char*)payload,"{\"cols\":%d,\"rows\":%d}",&cols,&rows)==2&&cols>0){
                    struct winsize ws2={0};ws2.ws_col=(unsigned short)cols;ws2.ws_row=(unsigned short)rows;
                    ioctl(pty_master,TIOCSWINSZ,&ws2);}
                else (void)write(pty_master,payload,plen);}
            if(op==9&&payload&&plen){uint8_t pong[2+125]={0x8A,0};
                pong[1]=(uint8_t)(plen<125?plen:125);memcpy(pong+2,payload,pong[1]);
                (void)write(browser_fd,pong,2+pong[1]);}
            free(payload);}
        if(FD_ISSET(pty_master,&rd)){
            ssize_t n=read(pty_master,buf,sizeof(buf));if(n<=0)break;
            ws_write(browser_fd,buf,n);}}
}

static int ask_crun_console(const char *net, const char *vm,
                            char *sock, int sock_len)
{
    char path[512], req[512], resp[512], name[128];
    struct sockaddr_un addr;
    int fd, len, result = -1;
    snprintf(path, sizeof(path), "/var/lib/cloonix/%s/%s", net, CONSOLE_SOCK);
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
      {
      KERR("ERROR console socket errno=%d", errno);
      return -1;
      }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path)-1, "%s", path);
    if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)))
      KERR("ERROR cannot reach suid-power on %s errno=%d", path, errno);
    else
      {
      len = snprintf(req, sizeof(req), "console_crun name=%s", vm);
      if (write(fd, req, len + 1) != len + 1)
        KERR("ERROR console request for %s", vm);
      else
        {
        memset(resp, 0, sizeof(resp));
        len = read(fd, resp, sizeof(resp) - 1);
        if (len <= 0)
          KERR("ERROR no console response for %s", vm);
        else if (sscanf(resp, "console_crun_ok name=%127s sock=%299s",
                        name, sock) != 2)
          KERR("ERROR console refused for %s: %s", vm, resp);
        else if (strcmp(name, vm))
          KERR("ERROR console response for %s answered %s", vm, name);
        else
          result = 0;
        }
      }
    close(fd);
    (void)sock_len;
    return result;
}

static void proxy_to_sh(int browser_fd,
                        const char *method, const char *path,
                        const char *proto, const char *full_hdr,
                        int full_hdr_len)
{
    const char *after=path+strlen(SH_PREFIX);
    const char *s1=strchr(after,'/');
    char net[128]={0},kind[32]={0},vm[128]={0};
    if(!s1) snprintf(net,sizeof(net),"%s",after);
    else{
        size_t n=s1-after;if(n>=sizeof(net))goto sh_err;memcpy(net,after,n);s1++;
        const char *s2=strchr(s1,'/');
        if(!s2){if(strcmp(s1,"ws"))snprintf(kind,sizeof(kind),"%s",s1);}
        else{
            size_t k=s2-s1;if(k>=sizeof(kind))goto sh_err;memcpy(kind,s1,k);s2++;
            const char *s3=strchr(s2,'/');
            if(!s3){if(strcmp(s2,"ws"))snprintf(vm,sizeof(vm),"%s",s2);}
            else{size_t m=s3-s2;if(m<sizeof(vm))memcpy(vm,s2,m);}}}

    int is_ws=(strcasestr(full_hdr,"Upgrade: websocket")!=NULL);
    if(!is_ws){
        char file[1024];snprintf(file,sizeof(file),"%s/cloonix-ssh.html",g_www_dir);
        int f=open(file,O_RDONLY);
        if(f<0){
            KERR("ERROR pty: cannot open %s errno=%d", file, errno);
            http_fail(browser_fd, 404, "ssh page", "no cloonix-ssh.html");
            return;}
        struct stat st;fstat(f,&st);char hdr[256];
        int hlen=snprintf(hdr,sizeof(hdr),
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %lld\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",(long long)st.st_size);
        (void)write(browser_fd,hdr,hlen);char rbuf[BUF_SIZE];ssize_t rn;
        while((rn=read(f,rbuf,sizeof(rbuf)))>0)(void)write(browser_fd,rbuf,rn);
        close(f);return;}

    if(ws_handshake(browser_fd,full_hdr))
      {
      KERR("ERROR pty: websocket handshake failed");
      goto sh_err;
      }

    /* ip, port and password from the linked cloonix.cfg parser. */
    t_cloonix_conf_info *cnf = get_own_cloonix_conf_info();
    char cfg_pass[MSG_DIGEST_LEN] = {0};
    char ipport_sh[512];
    if (cnf)
      snprintf(cfg_pass, sizeof(cfg_pass), "%s", cnf->passwd);
    {char psx[512];
     snprintf(psx,sizeof(psx),"/tmp/cloonix_proxymous_%s/proxy_pmain.sock",net);
     if (access(psx,F_OK)==0)
       snprintf(ipport_sh,sizeof(ipport_sh),"%s",psx);
     else if (cnf)
       {
       struct in_addr ia;
       ia.s_addr = cnf->ip;
       snprintf(ipport_sh,sizeof(ipport_sh),"%s:%d",inet_ntoa(ia),cnf->port);
       }
     else
       snprintf(ipport_sh,sizeof(ipport_sh),"127.0.0.1:45211");}

    char crun_sock[512];
    const char *ssh_bin = pthexec_cxp_cli();
    const char *dtach_bin = pthexec_dtach_bin();

    /* Refuse anything that is not one of the three consoles, HERE, before the
       fork. */
    if (((strcmp(kind,"ssh")) && (strcmp(kind,"shk")) && (strcmp(kind,"crun")))
        || (!vm[0]))
      {
      KERR("ERROR pty: refused console kind=[%s] vm=[%s] on net %s: a console "
           "is ssh, shk or crun, and needs a guest name",
           kind[0]?kind:"(none)", vm[0]?vm:"(none)", net);
      goto sh_err;
      }

    /* Before the fork: this process is multithreaded and the child may only
       call async-signal-safe functions before exec. */
    memset(crun_sock, 0, sizeof(crun_sock));
    if (!strcmp(kind,"crun"))
      {
      if (ask_crun_console(net, vm, crun_sock, sizeof(crun_sock)))
        {
        KERR("ERROR pty: no console for container %s on net %s", vm, net);
        goto sh_err;
        }
      }

    char passenv[MSG_DIGEST_LEN+32];
    snprintf(passenv,sizeof(passenv),"CLOONIX_PASSWD=%s",cfg_pass);
    int pty_master=-1;
    pid_t child=forkpty(&pty_master,NULL,NULL,NULL);
    if(child<0){KERR("WARNING shell: forkpty: %s",strerror(errno));goto sh_err;}
    if(child==0){
        /* The password travels in the environment, not on argv where ps would
           show it to every user of the machine. */
        char *env[]={"TERM=xterm-256color","LANG=C","LC_ALL=C",passenv,NULL};
        if(!strcmp(kind,"ssh")&&vm[0])
            execve(ssh_bin,(char*[]){(char*)ssh_bin,ipport_sh,"-",vm,NULL},env);
        else if(!strcmp(kind,"shk")&&vm[0]){
            char dsock[600];
            snprintf(dsock,sizeof(dsock),"/var/lib/cloonix/%s/dtach/%s",net,vm);
            /* E not -e "" */
            execve(dtach_bin,(char*[]){(char*)dtach_bin,"-a",dsock,"-E","-z",NULL},env);}
        else if(!strcmp(kind,"crun")&&vm[0]){
            execve(dtach_bin,(char*[]){(char*)dtach_bin,"-a",crun_sock,"-E","-z",NULL},env);}
        _exit(1);}

    spawn_child_watchdog(child);
    ws_pty_bridge(browser_fd,pty_master);
    kill(child,SIGHUP);close(pty_master);waitpid(child,NULL,WNOHANG);
    return;
sh_err:{const char *e="HTTP/1.1 500 Internal Server Error\r\nContent-Length:0\r\nConnection: close\r\n\r\n";
     (void)write(browser_fd,e,strlen(e));}
}

