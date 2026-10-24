/* web_ws.c — WebSocket primitives, client registries and broadcast.
 * Part of the GUI server single translation unit; see web_common.h. */

#include "web_common.h"

int  g_srv_fd = -1;

static const char *http_reason(int code)
{
    switch(code)
      {
      case 400: return "Bad Request";
      case 401: return "Unauthorized";
      case 403: return "Forbidden";
      case 404: return "Not Found";
      case 409: return "Conflict";
      case 500: return "Internal Server Error";
      case 502: return "Bad Gateway";
      case 503: return "Service Unavailable";
      default:  return "Error";
      }
}

void http_ok(int fd, const char *body)
{
    char rsp[1024];
    int len;
    if (!body)
        body = "";
    len = snprintf(rsp, sizeof(rsp),
                   "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: %d\r\nConnection: close\r\n\r\n%s",
                   (int) strlen(body), body);
    if (len > 0)
        (void) write(fd, rsp, len);
}

void http_fail(int fd, int code, const char *ctx, const char *body)
{
    char rsp[1024];
    int len;
    if (!body)
        body = "";
    len = snprintf(rsp, sizeof(rsp),
                   "HTTP/1.1 %d %s\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: %d\r\n"
                   "Connection: close\r\n\r\n%s",
                   code, http_reason(code), (int) strlen(body), body);
    if (len > 0)
        (void) write(fd, rsp, len);
    /* A NULL ctx means the caller has already said it, with more detail than
       is available here. */
    if (ctx)
        KERR("WARNING http %d %s: %s", code, ctx, body[0] ? body : http_reason(code));
}
char g_www_dir[512];
char g_snf_dir[512];
char g_webshark_bin[512];
char g_user_demo_dir[512];   /* /usr/share/cloonix/demos — demo storage */
char g_share_dir[512];       /* browse root (cannot escape upward) */

#define MAX_TOPO_WS_CLIENTS 64
static int             g_topo_ws_fds[MAX_TOPO_WS_CLIENTS];
static int             g_topo_ws_n   = 0;
static pthread_mutex_t g_topo_ws_mtx = PTHREAD_MUTEX_INITIALIZER;

static void topo_ws_add(int fd)
{
    pthread_mutex_lock(&g_topo_ws_mtx);
    if (g_topo_ws_n < MAX_TOPO_WS_CLIENTS) {
        g_topo_ws_fds[g_topo_ws_n++] = fd;
    } else {
        KERR("WARNING topo websocket: the maximum of clients is reached, fd %d not served", fd);
    }
    pthread_mutex_unlock(&g_topo_ws_mtx);
}

static void topo_ws_remove(int fd)
{
    pthread_mutex_lock(&g_topo_ws_mtx);
    for (int i = 0; i < g_topo_ws_n; i++) {
        if (g_topo_ws_fds[i] == fd) {
            g_topo_ws_fds[i] = g_topo_ws_fds[--g_topo_ws_n];
            break;
        }
    }
    pthread_mutex_unlock(&g_topo_ws_mtx);
}

int topo_ws_broadcast(const char *msg, int len)
{
    int fds[MAX_TOPO_WS_CLIENTS];
    int i, n;

    pthread_mutex_lock(&g_topo_ws_mtx);
    n = g_topo_ws_n;
    for (i = 0; i < n; i++)
        fds[i] = g_topo_ws_fds[i];
    pthread_mutex_unlock(&g_topo_ws_mtx);

    for (i = 0; i < n; i++)
        ws_send_text(fds[i], msg, len);
    return n;      /* how many browsers heard it */
}

static pthread_mutex_t g_ws_write_mtx = PTHREAD_MUTEX_INITIALIZER;

void ws_write_frame(int fd, const uint8_t *hdr, int hlen,
                    const char *msg, int len)
{
    struct iovec iov[2];
    int nb = 1;
    iov[0].iov_base = (void *) hdr;
    iov[0].iov_len  = hlen;
    if ((msg) && (len > 0)) {
        iov[1].iov_base = (void *) msg;
        iov[1].iov_len  = len;
        nb = 2;
    }
    pthread_mutex_lock(&g_ws_write_mtx);
    (void) writev(fd, iov, nb);
    pthread_mutex_unlock(&g_ws_write_mtx);
}

void ws_send_text(int fd, const char *msg, int len)
{
    uint8_t hdr[10];
    int hlen;
    if (len < 126) {
        hdr[0] = 0x81; /* FIN + text opcode */
        hdr[1] = (uint8_t)len;
        hlen = 2;
    } else if (len < 65536) {
        hdr[0] = 0x81;
        hdr[1] = 126;
        hdr[2] = (uint8_t)(len >> 8);
        hdr[3] = (uint8_t)(len & 0xFF);
        hlen = 4;
    } else {
        hdr[0] = 0x81;
        hdr[1] = 127;
        for (int i = 0; i < 8; i++)
            hdr[2+i] = (uint8_t)(((uint64_t)len >> (56 - 8*i)) & 0xFF);
        hlen = 10;
    }
    ws_write_frame(fd, hdr, hlen, msg, len);
}

void ws_broadcast_topo_json(const char *json, int len)
{
    pthread_mutex_lock(&g_topo_ws_mtx);
    int i = 0;
    while (i < g_topo_ws_n) {
        int fd = g_topo_ws_fds[i];
        uint8_t hdr[4] = {0x81, 0, 0, 0};
        int hlen = 2;
        if (len < 126) {
            hdr[1] = (uint8_t)len;
        } else {
            hdr[1] = 126;
            hdr[2] = (uint8_t)(len >> 8);
            hdr[3] = (uint8_t)(len & 0xFF);
            hlen = 4;
        }
        /* Through ws_write_frame like every other frame: header and payload
           in one writev, and no other thread writing between them. */
        ws_write_frame(fd, hdr, hlen, json, len);
        i++;
    }
    pthread_mutex_unlock(&g_topo_ws_mtx);
}

static int             g_images_ws_fds[MAX_TOPO_WS_CLIENTS];
static int             g_images_ws_n   = 0;
static pthread_mutex_t g_images_ws_mtx = PTHREAD_MUTEX_INITIALIZER;

static void images_ws_add(int fd)
{
    pthread_mutex_lock(&g_images_ws_mtx);
    if (g_images_ws_n < MAX_TOPO_WS_CLIENTS)
        g_images_ws_fds[g_images_ws_n++] = fd;
    pthread_mutex_unlock(&g_images_ws_mtx);
}

static void images_ws_remove(int fd)
{
    pthread_mutex_lock(&g_images_ws_mtx);
    for (int i = 0; i < g_images_ws_n; i++) {
        if (g_images_ws_fds[i] == fd) {
            g_images_ws_fds[i] = g_images_ws_fds[--g_images_ws_n];
            break;
        }
    }
    pthread_mutex_unlock(&g_images_ws_mtx);
}

void ws_broadcast_images_json(const char *json, int len)
{
    pthread_mutex_lock(&g_images_ws_mtx);
    int i = 0;
    while (i < g_images_ws_n) {
        int fd = g_images_ws_fds[i];
        pthread_mutex_unlock(&g_images_ws_mtx);
        ws_send_text(fd, json, len);
        pthread_mutex_lock(&g_images_ws_mtx);
        i++;
    }
    pthread_mutex_unlock(&g_images_ws_mtx);
}
#include <stdint.h>
static void sha1(const uint8_t *msg, size_t len, uint8_t out[20])
{
    uint32_t h0=0x67452301,h1=0xEFCDAB89,h2=0x98BADCFE,h3=0x10325476,h4=0xC3D2E1F0;
    uint64_t bit_len=(uint64_t)len*8;
    size_t padlen=len+1+8;
    if(padlen%64!=0)padlen+=64-(padlen%64);
    uint8_t *buf=calloc(padlen,1);
    memcpy(buf,msg,len);buf[len]=0x80;
    for(int i=0;i<8;i++)buf[padlen-8+i]=(uint8_t)(bit_len>>(56-i*8));
    for(size_t chunk=0;chunk<padlen;chunk+=64){
        uint32_t w[80];
        for(int i=0;i<16;i++)w[i]=((uint32_t)buf[chunk+i*4]<<24)|((uint32_t)buf[chunk+i*4+1]<<16)|((uint32_t)buf[chunk+i*4+2]<<8)|buf[chunk+i*4+3];
        for(int i=16;i<80;i++){uint32_t t=w[i-3]^w[i-8]^w[i-14]^w[i-16];w[i]=(t<<1)|(t>>31);}
        uint32_t a=h0,b=h1,c=h2,d=h3,e=h4;
        for(int i=0;i<80;i++){uint32_t f,k;
            if(i<20){f=(b&c)|((~b)&d);k=0x5A827999;}
            else if(i<40){f=b^c^d;k=0x6ED9EBA1;}
            else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8F1BBCDC;}
            else{f=b^c^d;k=0xCA62C1D6;}
            uint32_t t=((a<<5)|(a>>27))+f+e+k+w[i];
            e=d;d=c;c=(b<<30)|(b>>2);b=a;a=t;}
        h0+=a;h1+=b;h2+=c;h3+=d;h4+=e;}
    free(buf);
    for(int i=0;i<4;i++){out[0+i]=(h0>>(24-i*8))&0xff;out[4+i]=(h1>>(24-i*8))&0xff;
        out[8+i]=(h2>>(24-i*8))&0xff;out[12+i]=(h3>>(24-i*8))&0xff;out[16+i]=(h4>>(24-i*8))&0xff;}
}
static const char b64c[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void base64_enc(const uint8_t *in,size_t len,char *out)
{
    size_t i=0,j=0;
    for(;i+2<len;i+=3){out[j++]=b64c[in[i]>>2];out[j++]=b64c[((in[i]&3)<<4)|(in[i+1]>>4)];out[j++]=b64c[((in[i+1]&0xf)<<2)|(in[i+2]>>6)];out[j++]=b64c[in[i+2]&0x3f];}
    if(i+1==len){out[j++]=b64c[in[i]>>2];out[j++]=b64c[(in[i]&3)<<4];out[j++]='=';out[j++]='=';}
    else if(i+2==len){out[j++]=b64c[in[i]>>2];out[j++]=b64c[((in[i]&3)<<4)|(in[i+1]>>4)];out[j++]=b64c[(in[i+1]&0xf)<<2];out[j++]='=';}
    out[j]=0;
}

/* proto: the Sec-WebSocket-Protocol line, empty when none was asked. */
static int ws_accept(int fd, const char *full_hdr, const char *proto)
{
    char ws_key[128]={0};
    const char *kp=strcasestr(full_hdr,"Sec-WebSocket-Key:");
    if(!kp)return -1;
    kp+=18;while(*kp==' ')kp++;
    int ki=0;while(*kp&&*kp!='\r'&&*kp!='\n'&&ki<127)ws_key[ki++]=*kp++;
    while(ki>0&&(ws_key[ki-1]==' '||ws_key[ki-1]=='\t'))ws_key[--ki]=0;
    char combined[256];
    snprintf(combined,sizeof(combined),"%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11",ws_key);
    uint8_t sha_out[20];sha1((uint8_t*)combined,strlen(combined),sha_out);
    char accept[64];base64_enc(sha_out,20,accept);
    char resp[512];
    int rlen=snprintf(resp,sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n%s\r\n",accept,proto);
    return write(fd,resp,rlen)==rlen?0:-1;
}
static int ws_handshake(int fd, const char *full_hdr)
{
    return ws_accept(fd, full_hdr, "Sec-WebSocket-Protocol: binary\r\n");
}
static int ws_write(int fd, const void *data, size_t len)
{
    uint8_t hdr[10];int hlen;
    hdr[0]=0x82;
    if(len<126){hdr[1]=(uint8_t)len;hlen=2;}
    else if(len<65536){hdr[1]=126;hdr[2]=(len>>8)&0xff;hdr[3]=len&0xff;hlen=4;}
    else{hdr[1]=127;for(int i=0;i<8;i++)hdr[2+i]=(len>>((7-i)*8))&0xff;hlen=10;}
    if(write(fd,hdr,hlen)!=hlen)return -1;
    if(write(fd,data,len)!=(ssize_t)len)return -1;
    return 0;
}
static int ws_read_frame(int fd, uint8_t **buf_out, uint64_t *plen_out)
{
    uint8_t hdr[14];
    if(read(fd,hdr,2)!=2)return -1;
    int opcode=hdr[0]&0x0f,masked=(hdr[1]&0x80)!=0;
    uint64_t plen=hdr[1]&0x7f;
    if(plen==126){if(read(fd,hdr+2,2)!=2)return -1;plen=((uint64_t)hdr[2]<<8)|hdr[3];}
    else if(plen==127){if(read(fd,hdr+2,8)!=8)return -1;
        plen=0;for(int i=0;i<8;i++)plen=(plen<<8)|hdr[2+i];}
    uint8_t mask[4]={0};
    if(masked&&read(fd,mask,4)!=4)return -1;
    *plen_out=plen;
    if(plen==0){*buf_out=NULL;return opcode;}
    *buf_out=malloc(plen+1);if(!*buf_out)return -1;
    uint64_t got=0;
    while(got<plen){ssize_t n=read(fd,(*buf_out)+got,plen-got);
        if(n<=0){free(*buf_out);*buf_out=NULL;return -1;}got+=n;}
    if(masked)for(uint64_t i=0;i<plen;i++)(*buf_out)[i]^=mask[i%4];
    (*buf_out)[plen]=0;
    return opcode;
}

