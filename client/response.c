/*
 * frontier client response handler
 * 
 * Author: Sergey Kosyakov
 *
 * $Id$
 *
 *  Copyright (C) 2007  Fermilab
 *
 *  This program is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation, either version 3 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <string.h>
#include <frontier_client/frontier.h>
#include "fn-internal.h"
#include "expat.h"

extern void *(*frontier_mem_alloc)(size_t size);
extern void (*frontier_mem_free)(void *ptr);
 

static void XMLCALL
xml_cdata(void *userData,const XML_Char *s,int len)
 {
  FrontierResponse *fr=(FrontierResponse*)userData;
  
  frontierPayload_append(fr->payload[fr->payload_num-1],s,len);

  //printf("xml_cdata\n");
 }


static void XMLCALL
xml_startElement(void *userData,const char *name,const char **atts)
 {
  int i;
  FrontierResponse *fr=(FrontierResponse*)userData;

  //printf("xml_start %s\n",name);

  if(strcmp(name,"keepalive")==0)
    fr->keepalives++;
  else if(fr->keepalives!=0)
   {
    frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"received %d keepalives",fr->keepalives);
    fr->keepalives=0;
   }
  
  if(strcmp(name,"global_error")==0)
   {
    fr->error=-1;
    frontier_setErrorMsg(__FILE__,__LINE__,"Server has signalled Global Error [%s]",atts[1]);
    return;
   }  

  if(strcmp(name,"frontier")==0)
   {
    char buf[80];
    int i;
    size_t l=0;
    buf[0] = '\0';
    for(i=0;atts[i];i+=2)
     {
      l+=snprintf(&buf[l],sizeof(buf)-l," %s=%s",atts[i],atts[i+1]);
      if (l>=sizeof(buf))
	break;
     }
    frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"frontier server%s",buf);
    return;
   }

  if(strcmp(name,"payload")==0)
   {
    fr->payload_num++;
    fr->payload[fr->payload_num-1]=frontierPayload_create();
    fr->p_state=FNTR_WITHIN_PAYLOAD;
    for(i=0;atts[i];i+=2)
     {
      if(strcmp(atts[i],"encoding")==0)
       {
        fr->payload[fr->payload_num-1]->encoding=frontier_str_copy(atts[i+1]);
	continue;
       }      
     }
    return;
   }

  if(strcmp(name,"data")==0)
   {
    XML_SetCharacterDataHandler(fr->parser,xml_cdata);
    return;
   }   
   
  if(strcmp(name,"quality")==0 && fr->p_state==FNTR_WITHIN_PAYLOAD)
   {
    for(i=0;atts[i];i+=2)
     {
      //printf("attr <%s><%s>\n",atts[i],atts[i+1]);
      fflush(stdout);
      if(strcmp(atts[i],"error")==0)
       {
        fr->payload[fr->payload_num-1]->error_code=atoi(atts[i+1]);
	continue;
       }      
      if(strcmp(atts[i],"message")==0)
       {
        fr->payload[fr->payload_num-1]->error_msg=frontier_str_copy(atts[i+1]);
	continue;
       }            
      if(strcmp(atts[i],"records")==0)
       {
	//printf("Number of records: %d\n", atoi(atts[i+1]));
        fr->payload[fr->payload_num-1]->nrec=atoi(atts[i+1]);
	continue;
       }
      if(strcmp(atts[i],"full_size")==0)
       {
        fr->payload[fr->payload_num-1]->full_size=atoi(atts[i+1]);
	continue;
       }
      if(strcmp(atts[i],"md5")==0)
       {
        bcopy(atts[i+1],fr->payload[fr->payload_num-1]->srv_md5_str,32);
	fr->payload[fr->payload_num-1]->srv_md5_str[32]=0;
	continue;       
       }
     }
   }
 }


static void XMLCALL
xml_endElement(void *userData,const char *name)
 {
  int ret;
  
  FrontierResponse *fr=(FrontierResponse*)userData;

  //printf("xml_end %s\n",name);

  if(strcmp(name,"data")==0)
   {
    XML_SetCharacterDataHandler(fr->parser,(void*)0);
    return;
   }

  if(strcmp(name,"payload")==0)
   {
    ret=frontierPayload_finalize(fr->payload[fr->payload_num-1]);    
    fr->p_state=0;
    fr->error=ret;
   }
 }



FrontierResponse *frontierResponse_create(int *ec)
 {
  FrontierResponse *fr;
  int i;

  fr=frontier_mem_alloc(sizeof(FrontierResponse));
  if(!fr) 
   {
    *ec=FRONTIER_EMEM;
    frontier_setErrorMsg(__FILE__,__LINE__,"No more memory");
    return fr;
   }

  fr->error=0;
  fr->payload_num=0;
  fr->error_payload_ind=-1;
  fr->keepalives=0;

  fr->parser=XML_ParserCreate(NULL);
  if(!fr->parser)
   {
    frontier_mem_free(fr);
    *ec=FRONTIER_EUNKNOWN;
    frontier_setErrorMsg(__FILE__,__LINE__,"Can not create XML parser instance.");
    return (void*)0;
   }
  XML_SetUserData(fr->parser,fr);
  XML_SetElementHandler(fr->parser,xml_startElement,xml_endElement);

  fr->p_state=0;

  for(i=0;i<FRONTIER_MAX_PAYLOADNUM;i++)
   {
    fr->payload[i]=(void*)0;
   }

  return fr;
 }


void frontierResponse_delete(FrontierResponse *fr)
 {
  int i;

  if(!fr) return;

  if(fr->parser)
   {
    XML_SetUserData(fr->parser,(void*)0);
    XML_ParserFree(fr->parser);
   }

  for(i=0;i<fr->payload_num;i++)
   {
    frontierPayload_delete(fr->payload[i]);
   }
  
  frontier_mem_free(fr);
 }


int FrontierResponse_append(FrontierResponse *fr,char *buf,int len)
 {
  if(XML_Parse(fr->parser,buf,len,0)==XML_STATUS_ERROR) 
   {
    int xml_err=XML_GetErrorCode(fr->parser);
    frontier_setErrorMsg(__FILE__,__LINE__,"XML parse error %d:%s at line %d",xml_err,XML_ErrorString(xml_err),XML_GetCurrentLineNumber(fr->parser));
    return FRONTIER_EPROTO;
   }
   
  if(fr->error)
   {
    return fr->error;
   }
   
  return FRONTIER_OK;
 }



int frontierResponse_finalize(FrontierResponse *fr)
 {
  int i;
  if(XML_Parse(fr->parser,"",0,1)==XML_STATUS_ERROR) 
   {
    int xml_err=XML_GetErrorCode(fr->parser);
    frontier_setErrorMsg(__FILE__,__LINE__,"XML parse error %d:%s at line %d",xml_err,XML_ErrorString(xml_err),XML_GetCurrentLineNumber(fr->parser));
    return FRONTIER_EPROTO;
   }

  XML_SetUserData(fr->parser,(void*)0);
  XML_ParserFree(fr->parser);
  fr->parser=(void*)0;
  
  for(i=0;i<fr->payload_num;i++)
   {
    //printf("%d r:[%s] l:[%s]\n",i,fr->payload[i]->srv_md5_str,fr->payload[i]->md5_str);
    frontier_log(FRONTIER_LOGLEVEL_DEBUG,__FILE__,__LINE__,"Payload[%d] error %d error code %d",(i+1),fr->payload[i]->error,fr->payload[i]->error_code);
    if(fr->payload[i]->error) 
     {
      frontier_setErrorMsg(__FILE__,__LINE__,"Payload[%d] got error %d",(i+1),fr->payload[i]->error);
      return fr->payload[i]->error;
     }    
    if(fr->payload[i]->error_code) 
     {
      frontier_setErrorMsg(__FILE__,__LINE__,"Payload[%d] has error code %d",(i+1),fr->payload[i]->error_code);
      return FRONTIER_EPROTO;
     }
    if(strncmp(fr->payload[i]->srv_md5_str,fr->payload[i]->md5_str,32)) 
     {
      frontier_setErrorMsg(__FILE__,__LINE__,"Payload[%d]: MD5 hash mismatch: server [%s], local [%s]",(i+1),fr->payload[i]->srv_md5_str,fr->payload[i]->md5_str);
      return FRONTIER_EPROTO;
     }
   }

  return FRONTIER_OK;
 }







