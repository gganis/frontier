/*
 * frontier client http implementation
 * 
 * Author: Sergey Kosyakov
 *
 * $Id$
 *
 * Copyright (c) 2009, FERMI NATIONAL ACCELERATOR LABORATORY
 * All rights reserved. 
 *
 * For details of the Fermitools (BSD) license see Fermilab-2009.txt or
 *  http://fermitools.fnal.gov/about/terms.html
 *
 */
 
#include <fn-htclient.h>
#include "../fn-internal.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_NAME_LEN	256
#define URL_FMT_SRV	"http://%127[^/]/%127s"
#define URL_FMT_PROXY	"http://%s"

#define PERSISTCONNECTION 

FrontierHttpClnt *frontierHttpClnt_create(int *ec)
 {
  FrontierHttpClnt *c;
  
  c=frontier_mem_alloc(sizeof(FrontierHttpClnt));
  if(!c)
   {
    *ec=FRONTIER_EMEM;
    FRONTIER_MSG(*ec);
    return c;
   }
  bzero(c,sizeof(FrontierHttpClnt));
  c->socket=-1;
  c->frontier_id=(char*)0;
  c->data_pos=0;
  c->data_size=0;
  c->content_length=-1;
  c->age=-1;
  c->url_suffix="";
  c->rand_seed=getpid();
  c->max_age=-1;
  
  *ec=FRONTIER_OK;
  return c;
 }
 
void frontierHttpClnt_setConnectTimeoutSecs(FrontierHttpClnt *c,int timeoutsecs)
 {
  c->connect_timeout_secs=timeoutsecs;
 }
 
void frontierHttpClnt_setReadTimeoutSecs(FrontierHttpClnt *c,int timeoutsecs)
 {
  c->read_timeout_secs=timeoutsecs;
 }
 
void frontierHttpClnt_setWriteTimeoutSecs(FrontierHttpClnt *c,int timeoutsecs)
 {
  c->write_timeout_secs=timeoutsecs;
 }
 
int frontierHttpClnt_addServer(FrontierHttpClnt *c,const char *url)
 {
  FrontierUrlInfo *fui;
  int ec=FRONTIER_OK;
  
  fui=frontier_CreateUrlInfo(url,&ec);
  if(!fui) return ec;
  
  if(!fui->path)
   {
    frontier_setErrorMsg(__FILE__,__LINE__,"config error: server %s: servlet path is missing",fui->host);
    frontier_DeleteUrlInfo(fui);
    return FRONTIER_ECFG;
   }
   
  if(c->serveri.total>=(sizeof(c->server)/sizeof(c->server[0]))) 
   {
    frontier_setErrorMsg(__FILE__,__LINE__,"config error: server list is too long");
    frontier_DeleteUrlInfo(fui);    
    return FRONTIER_ECFG;
   }
  
  c->server[c->serveri.total]=fui;
  c->serveri.total++;

  return FRONTIER_OK;
 }

 
int frontierHttpClnt_addProxy(FrontierHttpClnt *c,const char *url)
 {
  FrontierUrlInfo *fui;
  int ec=FRONTIER_OK;
  
  fui=frontier_CreateUrlInfo(url,&ec);
  if(!fui) return ec;
  
  if(c->proxyi.total>=(sizeof(c->proxy)/sizeof(c->proxy[0])))
   {
    frontier_setErrorMsg(__FILE__,__LINE__,"config error: proxy list is too long");
    frontier_DeleteUrlInfo(fui);    
    return FRONTIER_ECFG;
   }
    
  c->proxy[c->proxyi.total]=fui;
  c->proxyi.total++;

  return FRONTIER_OK;
 }
 
 
 
void frontierHttpClnt_setCacheRefreshFlag(FrontierHttpClnt *c,int refresh_flag)
 {
  c->refresh_flag=refresh_flag;
 }

void frontierHttpClnt_setCacheMaxAgeSecs(FrontierHttpClnt *c,int secs)
 {
  c->max_age=secs;
 }
 
void frontierHttpClnt_setPreferIpFamily(FrontierHttpClnt *c,int ipfamily)
 {
  c->prefer_ip_family=ipfamily;
 }
 
void frontierHttpClnt_setUrlSuffix(FrontierHttpClnt *c,char *suffix)
 {
  /* note that no copy is made -- caller must insure longevity of suffix */
  c->url_suffix=suffix;
 }

 
void frontierHttpClnt_setFrontierId(FrontierHttpClnt *c,const char *frontier_id)
 {
  int i;
  int len;
  char *p;
  unsigned char ch;
  
  len=strlen(frontier_id);
  if(len>MAX_NAME_LEN) len=MAX_NAME_LEN;
  
  if(c->frontier_id) frontier_mem_free(c->frontier_id);
  c->frontier_id=frontier_mem_alloc(len+1);
  p=c->frontier_id;
  
  // eliminate any illegal characters
  for(i=0;i<len;i++)  
   {
    ch=frontier_id[i];
    if(isalnum(ch)||strchr(" ;:,.<>/?=+-_{}[]()|@",ch))
      *p++=ch;
   }
  *p=0;
 } 
 
 
 
static int http_read(FrontierHttpClnt *c)
 {
  //printf("\nhttp_read\n");
  //bzero(c->buf,FRONTIER_HTTP_BUF_SIZE);
  c->data_pos=0;  
  c->data_size=frontier_read(c->socket,c->buf,FRONTIER_HTTP_BUF_SIZE,c->read_timeout_secs,c->cur_ai);

  return c->data_size;
 }

 
static int read_char(FrontierHttpClnt *c,char *ch)
 {
  int ret;
  
  if(c->data_pos+1>c->data_size)
   {
    ret=http_read(c);
    if(ret<=0) return ret;
   }
  *ch=c->buf[c->data_pos];
  c->data_pos++;
  return 1;
 }

 
static int test_char(FrontierHttpClnt *c,char *ch)
 {
  int ret;
  
  if(c->data_pos+1>c->data_size)
   {
    ret=http_read(c);
    if(ret<=0) return ret;
   }
  *ch=c->buf[c->data_pos];
  return 1;
 }
 
 
static int read_line(FrontierHttpClnt *c,char *buf,int buf_len)
 {
  int i;
  int ret;
  char ch,next_ch;
  
  i=0;
  while(1)
   {
    ret=read_char(c,&ch);
    if(ret<0) return ret;
    if(!ret) break;
    
    if(ch=='\r' || ch=='\n')
     {
      ret=test_char(c,&next_ch);
      if(ret<0) return ret;
      if(!ret) break;      
      if(ch=='\r' && next_ch=='\n') read_char(c,&ch);	// Just ignore
      break;
     }
    buf[i]=ch;
    i++;
    if(i>=buf_len-1) break;
   }
  buf[i]=0;
  return i;
 }
   
  
static int open_connection(FrontierHttpClnt *c)
 {
  int ret;
  struct addrinfo *ai;
  FrontierUrlInfo *fui_proxy,*fui_server,*fui;
  in_port_t port;
  
  if(c->socket!=-1)
   {
    frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"reusing persisted connection s=%d",c->socket);
    return 0;
   }

  frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"no persisted connection found, opening new");

  fui_proxy=c->proxy[c->proxyi.cur];
  fui_server=c->server[c->serveri.cur];
  
  if(!fui_server)
   {
    frontier_setErrorMsg(__FILE__,__LINE__,"no available server");
    return FRONTIER_ECFG;
   }   
  
  if(fui_proxy)
   {
    frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"connecting to proxy %s",fui_proxy->host);
    fui=fui_proxy;
    c->using_proxy=1;
   }
  else
   {
    frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"connecting to server %s",fui_server->host);
    fui=fui_server;
    c->using_proxy=0;
   }
     
  ret=frontier_resolv_host(fui,c->prefer_ip_family);
  if(ret) return ret;
  ai=fui->fai->ai;

  c->socket=frontier_socket(ai->ai_addr->sa_family);
  if(c->socket<0) return c->socket;
  
  port=htons((unsigned short)(fui->port));
  if(ai->ai_addr->sa_family==AF_INET6)
   ((struct sockaddr_in6*)(ai->ai_addr))->sin6_port=port;
  else
   ((struct sockaddr_in*)(ai->ai_addr))->sin_port=port;

  ret=frontier_connect(c->socket,ai->ai_addr,ai->ai_addrlen,c->connect_timeout_secs);

  if(ret!=FRONTIER_OK)
   {
    frontier_socket_close(c->socket);
    c->socket=-1;
   }

  c->cur_ai=ai;
   
  return ret;
}


#define FN_REQ_BUF 8192

static int get_url(FrontierHttpClnt *c,const char *url,int is_post)
{
  int ret;
  int len;  
  char buf[FN_REQ_BUF];
  FrontierUrlInfo *fui_server;
  char *http_method;
  
  http_method=is_post?"POST":"GET";

  fui_server=c->server[c->serveri.cur];

  bzero(buf,FN_REQ_BUF);
  
  if(c->using_proxy)
   {
    len=snprintf(buf,FN_REQ_BUF,"%s %s%s%s%s HTTP/1.0\r\nHost: %s\r\n",http_method,fui_server->url,*url?"/":"",url,c->url_suffix,fui_server->host);
   }
  else
   {
    len=snprintf(buf,FN_REQ_BUF,"%s /%s%s%s%s HTTP/1.0\r\nHost: %s\r\n",http_method,fui_server->path,*url?"/":"",url,c->url_suffix,fui_server->host);
   }
  if(len>=FN_REQ_BUF)
   {
    frontier_setErrorMsg(__FILE__,__LINE__,"request is bigger than %d bytes",FN_REQ_BUF);
    return FRONTIER_EIARG;
   }
  
  // Would use 'Cache-Control: max-stale=0' but squid 2.6 series (at least
  //  2.6STABLE13 and 2.6STABLE18, but not 2.7STABLE4) then sends just 
  //  'Cache-Control: max-stale' (without =0) upstream and messes up
  //  its parent.  max-stale=1 is almost the same thing except for one
  //  second, and requires squid to return an error if server is down
  //  rather than returning stale data.  This option has very little
  //  effect, but it gets sent upstream to frontier server which now
  //  can send back a corresponding stale-if-error header which does
  //  tell squid 2.7 or later to return a 504 error if the origin server
  //  can't be reached when a cached item is stale.  frontier_client
  //  couldn't handle that before this header was added, so this allows
  //  a compatible upgrade
  // Leave \r\n off because we'll be adding it below
  ret=snprintf(buf+len,FN_REQ_BUF-len,
  	"X-Frontier-Id: %s\r\nCache-Control: max-stale=1",c->frontier_id);
  if(ret>=FN_REQ_BUF-len)
   {
    frontier_setErrorMsg(__FILE__,__LINE__,"request is bigger than %d bytes",FN_REQ_BUF);
    return FRONTIER_EIARG;
   }
  len+=ret;

  // Cache-control: max-age=0 allows If-Modified-Since to re-validate cache,
  //  which can be much less stress on servers than Pragma: no-cache.
  if(c->refresh_flag==1)
    ret=snprintf(buf+len,FN_REQ_BUF-len,",max-age=0\r\n");
  else if((c->refresh_flag==0)&&(c->max_age>=0))
    ret=snprintf(buf+len,FN_REQ_BUF-len,",max-age=%d\r\n",c->max_age);
  else
    ret=snprintf(buf+len,FN_REQ_BUF-len,"\r\n");
  if(ret>=FN_REQ_BUF-len)
   {
    frontier_setErrorMsg(__FILE__,__LINE__,"request is bigger than %d bytes",FN_REQ_BUF);
    return FRONTIER_EIARG;
   }
  len+=ret;
   
  // POST is always no-cache
  if(is_post||(c->refresh_flag==2))
   {
    ret=snprintf(buf+len,FN_REQ_BUF-len,"Pragma: no-cache\r\n");
    if(ret>=FN_REQ_BUF-len)
     {
      frontier_setErrorMsg(__FILE__,__LINE__,"request is bigger than %d bytes",FN_REQ_BUF);
      return FRONTIER_EIARG;
     }
    len+=ret;
   }

#ifdef PERSISTCONNECTION
  ret=snprintf(buf+len,FN_REQ_BUF-len,"Connection: keep-alive\r\n\r\n");
#else
  ret=snprintf(buf+len,FN_REQ_BUF-len,"\r\n");
#endif
  if(ret>=FN_REQ_BUF-len)
   {
    frontier_setErrorMsg(__FILE__,__LINE__,"request is bigger than %d bytes",FN_REQ_BUF);
    return FRONTIER_EIARG;
   }
  len+=ret;
   
  frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"request <%s>",buf);
   
  ret=frontier_write(c->socket,buf,len,c->write_timeout_secs,c->cur_ai);
  if(ret<0) return ret;
  return FRONTIER_OK;
 }
 
 
static int read_connection(FrontierHttpClnt *c)
 {
  int ret;
  int tot=0;
  char buf[FN_REQ_BUF];
  
  // Read status line
  ret=read_line(c,buf,FN_REQ_BUF);
  if(ret<=0) return ret;
  tot=ret;
  frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"status line <%s>",buf);
  if((strncmp(buf,"HTTP/1.0 5",10)==0)||(strncmp(buf,"HTTP/1.1 5",10)==0))
   {
    /* 5xx HTTP error code indicates server error */
    frontier_setErrorMsg(__FILE__,__LINE__,"server error (%s) proxy=%s server=%s",
	buf,frontierHttpClnt_curproxyname(c),frontierHttpClnt_curservername(c));
    return FRONTIER_ESERVER;
   }

  if((strncmp(buf,"HTTP/1.0 200 ",13)!=0)&&(strncmp(buf,"HTTP/1.1 200 ",13)!=0))
   {
    frontier_setErrorMsg(__FILE__,__LINE__,"bad response (%s) proxy=%s server=%s",
	buf,frontierHttpClnt_curproxyname(c),frontierHttpClnt_curservername(c));
    return FRONTIER_EPROTO;
   }
      
  do
   {   
    ret=read_line(c,buf,FN_REQ_BUF);
    if(ret<0) return ret;
    tot+=ret;
    frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"got line <%s>",buf);
    /* look for "content-length" header */
#define CLHEAD "content-length: "
#define CLLEN (sizeof(CLHEAD)-1)
    if((c->content_length==-1)&&(ret>CLLEN)&&(strncasecmp(buf,CLHEAD,CLLEN)==0))
      c->content_length=atoi(buf+CLLEN);
#undef CLHEAD
#undef CLLEN
    /* look for "age" header */
#define AGEHEAD "age: "
#define AGELEN (sizeof(AGEHEAD)-1)
    else if((c->age==-1)&&(ret>AGELEN)&&(strncasecmp(buf,AGEHEAD,AGELEN)==0))
      c->age=atoi(buf+AGELEN);
#undef AGEHEAD
#undef AGELEN
   }while(*buf);

  return tot; 
 }
 

int frontierHttpClnt_open(FrontierHttpClnt *c)
 {
  int ret;
  
  ret=open_connection(c);
  return ret;
 }
 
 
int frontierHttpClnt_get(FrontierHttpClnt *c,const char *url)
 {
  return frontierHttpClnt_post(c,url,0);
 }


int frontierHttpClnt_post(FrontierHttpClnt *c,const char *url,const char *body)
 {
  int ret;
  int len=body?strlen(body):0;
  int try=0;

  for(try=0;try<2;try++)
   {
    ret=get_url(c,url,0);
    if(ret) return ret;
  
    if(len>0)
     {
      frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"body (%d bytes): [\n%s\n]",len,body);
      ret=frontier_write(c->socket,body,len,c->write_timeout_secs,c->cur_ai);
      if(ret<0) return ret;
     }

    ret=read_connection(c);
    if(ret==0)
     {
      if(try==0)
       {
        /*An empty response can happen on persisted connections after*/
	/*long waits.  Close, reopen, and retry.*/
        frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"got empty response, re-connecting and retrying");
        frontierHttpClnt_close(c);
        if(frontierHttpClnt_open(c)==FRONTIER_OK)continue;
        frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"re-connect failed");
       }
      frontier_setErrorMsg(__FILE__,__LINE__,"empty response from server proxy=%s server=%s",
	frontierHttpClnt_curproxyname(c),frontierHttpClnt_curservername(c));
      return FRONTIER_ENETWORK;    
     }
    if((ret==FRONTIER_ESYS)&&(try==0))
     {
      /*System error 104 Connection reset by peer has been seen on*/
      /*heavily-loaded localhost squids (on a node with 8 cores).*/
      /*Close, reopen, and retry.*/
      frontier_log(FRONTIER_LOGLEVEL_WARNING,__FILE__,__LINE__,"Retrying after system error");
      frontierHttpClnt_close(c);
      if(frontierHttpClnt_open(c)==FRONTIER_OK)continue;
      frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"re-connect failed");
     }
    /*if there was no continue, don't try again*/
    break;
   }

  if(ret>0)return FRONTIER_OK;
  return ret;
 }
 
 
int frontierHttpClnt_read(FrontierHttpClnt *c,char *buf,int buf_len)
 {
  int ret;
  int available;
  
#ifdef PERSISTCONNECTION
  if(c->content_length==0)
    /* no more data to read for this url */
    return 0;
#endif

  if(c->data_pos+1>c->data_size)
   {
    ret=http_read(c);
    if(ret<=0) return ret;
   }
   
  available=c->data_size-c->data_pos;
  available=buf_len>available?available:buf_len;
#ifdef PERSISTCONNECTION
  if(c->content_length>0)
   {
    if(available>c->content_length)
     {
      frontier_setErrorMsg(__FILE__,__LINE__,"More data available (%d bytes) than Content-Length (%d bytes) says should be there",available,c->content_length);
      return -1;
     }
    c->content_length-=available;
   }
  c->total_length+=available;
#endif
  bcopy(c->buf+c->data_pos,buf,available);
  c->data_pos+=available;

#if 0
   {
    /* print the beginning of the data in the block */
    char savech;
    int lineno;
    char *p = buf;
    for (lineno = 0; lineno < 15; lineno++)
     {
      if ((lineno+1) * 40 >= available)
       {
	break;
       }
      savech = p[40];
      p[40] = '\0';
      frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"data [%s]",p);
      p[40] = savech;
      p+=40;
     }
     if (lineno == 15)
        frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"(more data not logged)");
   }
#endif
    
  return available;
 }
 
 
void frontierHttpClnt_close(FrontierHttpClnt *c)
 {
#ifdef PERSISTCONNECTION
  if(c->content_length<0)
   {
    /*there was no Content-Length header or this function was called twice*/
    frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"closing persistent connection");
    /*note that "Proxy-Connection: close" was probably also set*/
    frontier_socket_close(c->socket);
    c->socket=-1;
   }
  else if(c->total_length>=FRONTIER_MAX_PERSIST_SIZE)
   {
    /*squid inconsistently drops persistent connections after large objects
      (at least in squid2.6STABLE13, it drops after those that were cache
      MISSes upstream when the object initially loaded), so make it consistent
      and close after every large object.  This helps with load balancing.*/
    frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"closing persistent connection after large object");
    frontier_socket_close(c->socket);
    c->socket=-1;
   }
  else
   {
    frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"persisting connection s=%d",c->socket);
   }
  c->total_length=0;
#else
  frontier_socket_close(c->socket);
  c->socket=-1;
#endif
  c->data_pos=0;
  c->data_size=0;  
  c->content_length=-1;
 }

void frontierHttpClnt_drop(FrontierHttpClnt *c)
 {
  if(c->socket!=-1)
   {
    // force the connection to drop
    frontier_socket_close(c->socket);
    c->socket=-1;
   }
 }
 
void frontierHttpClnt_delete(FrontierHttpClnt *c)
 {
  int i;
  
  if(!c) return;

  frontierHttpClnt_drop(c);
  
  for(i=0;i<(sizeof(c->server)/sizeof(c->server[0])); i++)
    frontier_DeleteUrlInfo(c->server[i]);
  for(i=0;i<(sizeof(c->proxy)/sizeof(c->proxy[0])); i++)
    frontier_DeleteUrlInfo(c->proxy[i]);
  
  if(c->frontier_id) frontier_mem_free(c->frontier_id);
  
  frontier_mem_free(c);
 }

void frontierHttpClnt_clear(FrontierHttpClnt *c)
 {
  // set so if there is a next time, it will reset the proxy & server lists
  c->proxyi.whenreset=1;
  c->serveri.whenreset=1;
 }

void frontierHttpClnt_resetwhenold(FrontierHttpClnt *c)
 {
  int i;
  time_t now;
  char nowbuf[26];

  now=time(0);
  if(c->proxyi.whenreset==0)
   {
    /*first time*/
    c->proxyi.whenreset=now;
    c->serveri.whenreset=now;
    return;
   }

  if((now-c->proxyi.whenreset)<FRONTIER_RESETPROXY_SECS)
   {
    /*not yet old*/
    return;
   }
  c->proxyi.whenreset=now;

  if(c->socket!=-1)
   {
    /*close persisting connection*/
    frontier_socket_close(c->socket);
    c->socket=-1;
   }

  if(((c->proxyi.num_balanced>0)&&(c->proxyi.cur>=c->proxyi.num_balanced))||
		(c->proxyi.cur>0))
  {
   //print warning if not in the first proxy group anymore
   frontier_log(FRONTIER_LOGLEVEL_WARNING,__FILE__,__LINE__,"Resetting the proxy list at %s",frontier_str_now(nowbuf));
  }

  c->proxyi.cur=c->proxyi.first=0;
  for(i=0;i<c->proxyi.total;i++)
    frontier_FreeAddrInfo(c->proxy[i]);

  if((now-c->serveri.whenreset)<FRONTIER_RESETSERVER_SECS)
   {
    /*servers not yet old*/
    return;
   }
  c->serveri.whenreset=now;

  if((c->serveri.num_balanced==0)&&(c->serveri.cur>0))
  {
   //print warning if not in the first server group anymore
   frontier_log(FRONTIER_LOGLEVEL_WARNING,__FILE__,__LINE__,"Resetting the server list at %s",frontier_str_now(nowbuf));
  }

  c->serveri.cur=c->serveri.first=0;
  for(i=0;i<c->serveri.total;i++)
    frontier_FreeAddrInfo(c->server[i]);

  return;
 }

int frontierHttpClnt_getContentLength(FrontierHttpClnt *c)
 {
  return c->content_length;
 }

int frontierHttpClnt_getCacheAgeSecs(FrontierHttpClnt *c)
 {
  return c->age;
 }

int frontierHttpClnt_shuffleproxygroup(FrontierHttpClnt *c)
 {
  if((c->socket!=-1)||(c->proxyi.cur>=c->proxyi.total))
   {
    /*don't shuffle if connection is persisting or not using a proxy*/
    if(c->proxyi.cur>=c->proxyi.total)
      return(-1);
    return c->proxyi.cur;
   }
  if((c->proxyi.num_balanced>0)&&(c->proxyi.cur<c->proxyi.num_balanced))
   {
    // randomize start if balancing was selected
    // ignore the possibility that the addresses may include round-robin
    //   when balancing
    int i,numgood=0;
    for(i=0;i<c->proxyi.num_balanced;i++)
      if(!c->proxy[i]->fai->haderror)
	numgood++;
    if(numgood>0)
     {
      c->proxyi.first=rand_r(&c->rand_seed)%numgood;
      // skip the ones that had an error
      for(i=0;i<=c->proxyi.first;i++)
	if(c->proxy[i]->fai->haderror)
	  c->proxyi.first++;
     }
    else c->proxyi.first=c->proxyi.num_balanced;
    c->proxyi.cur=c->proxyi.first;
   }
  else
   {
    // not in client-balancing group
    // select next good round-robin address if any
    FrontierUrlInfo *fui=c->proxy[c->proxyi.cur];
    FrontierAddrInfo *startfai=fui->fai;
    do
     {
      if ((fui->fai=fui->fai->next)==0)
	fui->fai=&fui->firstfai;
     } while(fui->fai->haderror&&(fui->fai!=startfai));
    fui->lastfai=fui->fai;
   }
  return c->proxyi.cur;
 }

int frontierHttpClnt_shuffleservergroup(FrontierHttpClnt *c)
 {
  if(c->socket!=-1)
   {
    /*don't shuffle anything if connection is persisting*/
    return c->serveri.cur;
   }
  if(c->serveri.num_balanced>0)
   {
    // randomize start if balancing was selected
    // ignore the possibility that these addresses may include round-robin
    int i,numgood=0;
    for(i=0;i<c->serveri.total;i++)
      if(!c->server[i]->fai->haderror)
	numgood++;
    if(numgood>0)
     {
      c->serveri.first=rand_r(&c->rand_seed)%numgood;
      // skip the ones that had an error
      for(i=0;i<=c->serveri.first;i++)
	if(c->server[i]->fai->haderror)
	  c->serveri.first++;
     }
    c->serveri.cur=c->serveri.first;
   }
  else
   {
    // not doing client-balancing
    // select next good round-robin address if any
    FrontierUrlInfo *fui=c->server[c->serveri.cur];
    FrontierAddrInfo *startfai=fui->fai;
    do
     {
      if ((fui->fai=fui->fai->next)==0)
	fui->fai=&fui->firstfai;
     } while(fui->fai->haderror&&(fui->fai!=startfai));
    fui->lastfai=fui->fai;
   }
  return c->serveri.cur;
 }

int frontierHttpClnt_resetproxygroup(FrontierHttpClnt *c)
 {
  FrontierUrlInfo *fui;
  if(c->proxyi.total==0)
   {
    /*not using any proxies*/
    return(-1);
   }
  if(c->socket!=-1)
   {
    /*close persisting connection*/
    frontier_socket_close(c->socket);
    c->socket=-1;
   }
  if((c->proxyi.num_balanced>0)&&(c->proxyi.cur<c->proxyi.num_balanced))
   {
    c->proxyi.cur=c->proxyi.first;
    if(c->proxyi.cur>=c->proxyi.total)
      return(-1);
    fui=c->proxy[c->proxyi.cur];
   }
  else
   {
    fui=c->proxy[c->proxyi.cur];
    fui->fai=fui->lastfai=&fui->firstfai;
   }
  if(fui->fai->haderror)
   {
    /*find one without an error*/
    return(frontierHttpClnt_nextproxy(c,0));
   }
  return(c->proxyi.cur);
 }

int frontierHttpClnt_resetserverlist(FrontierHttpClnt *c)
 {
  if(c->socket!=-1)
   {
    /*close persisting connection*/
    frontier_socket_close(c->socket);
    c->socket=-1;
   }
  c->serveri.cur=c->serveri.first;
  if(c->serveri.cur>=c->serveri.total)
    return(-1);
  if(c->server[c->serveri.cur]->fai->haderror)
   {
    /*find one without an error*/
    return(frontierHttpClnt_nextserver(c,0));
   }
  return(c->serveri.cur);
 }

int frontierHttpClnt_usinglastproxyingroup(FrontierHttpClnt *c)
 {
  FrontierUrlInfo *fui=c->proxy[c->proxyi.cur];
  FrontierAddrInfo *nextfai;
  int nextproxy;
  if((c->proxyi.num_balanced>0)&&(c->proxyi.cur<c->proxyi.num_balanced))
   {
    nextproxy=c->proxyi.cur+1;
    if(nextproxy==c->proxyi.num_balanced)
      nextproxy=0;
    if(nextproxy!=c->proxyi.first)
      return 0;
    return 1;
   }
  if ((nextfai=fui->fai->next)==0)
    nextfai=&fui->firstfai;
  if(fui->lastfai!=nextfai)
    return 0;
  return 1;
 }

int frontierHttpClnt_nextproxy(FrontierHttpClnt *c,int curhaderror)
 {
  FrontierUrlInfo *fui=c->proxy[c->proxyi.cur];
  if(c->socket!=-1)
   {
    frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"closing persistent connection while choosing next proxy");
    frontier_socket_close(c->socket);
    c->socket=-1;
   }
  if(curhaderror)
    fui->fai->haderror=1;
  // advance the round-robin if there is any other address
  if ((fui->fai=fui->fai->next)==0)
    fui->fai=&fui->firstfai;
  if(fui->lastfai==fui->fai)
   {
    /*end of round-robin, advance through proxy list*/
    c->proxyi.cur++;
    if(c->proxyi.num_balanced>0)
     {
      if(c->proxyi.cur==c->proxyi.num_balanced)
        /*wrap around when reach the last balanced proxy*/
        c->proxyi.cur=0;
      if(c->proxyi.cur==c->proxyi.first)
        /*done with balancing, set to the non-balanced ones, if any*/
        c->proxyi.cur=c->proxyi.num_balanced;
     }
   }
  if(c->proxyi.cur>=c->proxyi.total)
   {
    /* Exhausted list.  For each proxy in which all addresses have had
       errors, clear the errors for next try.  */
    int i;
    for(i=0;i<c->proxyi.total;i++)
     {
      FrontierAddrInfo *fai;
      int anygood=0;
      for(fai=&c->proxy[i]->firstfai;fai!=0;fai=fai->next)
       {
	if(!fai->haderror)
	  anygood=1;
	if(i<c->proxyi.num_balanced)
	  break; // other round-robin addresses are ignored when balancing
       }
      if(!anygood)
       {
        for(fai=&c->proxy[i]->firstfai;fai!=0;fai=fai->next)
	  fai->haderror=0;
       }
     }
    return(-1);
   }
  if(c->proxy[c->proxyi.cur]->fai->haderror)
    return(frontierHttpClnt_nextproxy(c,0));
  return(c->proxyi.cur);
 }

int frontierHttpClnt_nextserver(FrontierHttpClnt *c,int curhaderror)
 {
  FrontierUrlInfo *fui=c->server[c->serveri.cur];
  if(!c->using_proxy&&(c->socket!=-1))
   {
    frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"closing persistent connection while choosing next server");
    frontier_socket_close(c->socket);
    c->socket=-1;
   }
  // Note that if the host name has not been resolved because we're going
  // through a proxy, fui->fai will still be a valid FrontierAddrInfo, it
  // will just have ai=0 and next=0.
  if(curhaderror)
    fui->fai->haderror=1;
  // advance the round-robin if there is any other address
  if ((fui->fai=fui->fai->next)==0)
    fui->fai=&fui->firstfai;
  if(fui->lastfai==fui->fai)
   {
    /*end of server round-robin, advance through server list*/
    c->serveri.cur++;
    if(c->serveri.cur==c->serveri.total)
      /*wrap around in case doing load balancing*/
      c->serveri.cur=0;
    if(c->serveri.cur==c->serveri.first)
      /*set to total when done*/
      c->serveri.cur=c->serveri.total;
   }
  if(c->serveri.cur>=c->serveri.total)
   {
    /* Exhausted list.  For each server in which all addresses have had
       errors, clear the errors for possible next try.  */
    int i;
    for(i=0;i<c->serveri.total;i++)
     {
      FrontierAddrInfo *fai;
      int anygood=0;
      for(fai=&c->server[i]->firstfai;fai!=0;fai=fai->next)
       {
	if(!fai->haderror)
	  anygood=1;
	if(c->serveri.num_balanced>0)
	  break; // other round-robin addresses are ignored when balancing
       }
      if(!anygood)
       {
        for(fai=&c->server[i]->firstfai;fai!=0;fai=fai->next)
	  fai->haderror=0;
       }
     }
    return(-1);
   }
  if(((fui=c->server[c->serveri.cur])->fai!=0)&&fui->fai->haderror)
    return(frontierHttpClnt_nextserver(c,0));
  return(c->serveri.cur);
 }


static void gethostnameandaddr(FrontierUrlInfo *fui,char *buf,int len)
 {
  strncpy(buf,fui->host,len);
  if(fui->fai->ai!=0)
   {
    int n=strlen(fui->host);
    buf+=n;
    len-=n;
    snprintf(buf,len,"[%s]",frontier_ipaddr(fui->fai->ai->ai_addr));
    buf[len-1]='\0';
   }
 }

char *frontierHttpClnt_curproxyname(FrontierHttpClnt *c)
 {
  if (c->proxyi.cur<c->proxyi.total)
   {
    gethostnameandaddr(c->proxy[c->proxyi.cur],c->proxyi.buf,sizeof(c->proxyi.buf));
    return(c->proxyi.buf);
   }
  return "";
 }

char *frontierHttpClnt_curservername(FrontierHttpClnt *c)
 {
  if (c->serveri.cur<c->serveri.total)
   {
    gethostnameandaddr(c->server[c->serveri.cur],c->serveri.buf,sizeof(c->serveri.buf));
    return(c->serveri.buf);
   }
  return "";
 }

char *frontierHttpClnt_curserverpath(FrontierHttpClnt *c)
 {
  if (c->serveri.cur<c->serveri.total)
    return c->server[c->serveri.cur]->path;
  return "";
 }

char *frontierHttpClnt_myipaddr(FrontierHttpClnt *c)
 {
  // allocate a size big enough for ipv6, which will also work for ipv4
  struct sockaddr_in6 sockaddrbuf;
  socklen_t namelen=sizeof(sockaddrbuf);
  if(getsockname(c->socket, (struct sockaddr *)&sockaddrbuf, &namelen)<0)
   {
    frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"cannot get sockname for socket %d: %s",c->socket,strerror(errno));
    return NULL;
   }
  strncpy(c->serveri.buf,frontier_ipaddr((struct sockaddr *)&sockaddrbuf),sizeof(c->serveri.buf));
  frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"my ip addr: %s",c->serveri.buf);
  return(c->serveri.buf);
 }

void frontierHttpClnt_setNumBalancedProxies(FrontierHttpClnt *c,int num)
 {
  c->proxyi.num_balanced=num;
 }

void frontierHttpClnt_setBalancedServers(FrontierHttpClnt *c)
 {
  c->serveri.num_balanced=c->serveri.total;
 }

