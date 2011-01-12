/**
 * The main Frontier manager
 * $Id$
 *
 * Copyright (c) 2009, FERMI NATIONAL ACCELERATOR LABORATORY
 * All rights reserved. 
 *
 * For details of the Fermitools (BSD) license see Fermilab-2009.txt or
 *  http://fermitools.fnal.gov/about/terms.html
 */

package gov.fnal.frontier;

import javax.servlet.*;
import javax.servlet.http.*;
import java.util.*;
import java.text.SimpleDateFormat;
import java.io.*;


public final class Frontier
 {
  private static final SimpleDateFormat date_fmt=new SimpleDateFormat("MM/dd/yy HH:mm:ss.SSS z Z");    
  private static boolean initialized=false;    
  private static String conf_server_name="not_set";
  private static String conf_ds_name;
  private static String conf_xsd_table="";
  private static final int SHORTCACHE=0;
  private static final int LONGCACHE=1;
  private static final int NUMCACHELENGTHS=2;
  private static String[] conf_cache_expire_hourofday = new String[NUMCACHELENGTHS];
  private static String[] conf_cache_expire_seconds = new String[NUMCACHELENGTHS];
  
  private static String conf_monitor_node;  // MonAlisa stuff, node address events to be sent to
  private static String conf_monitor_delay; // MonAlisa stuff, delay between events sent,in msec
  private static Monitor monitor;           // MonAlisa stuff, wrapper class
  
  private static boolean use_fdo_cache;     // If true, the FDO info is created once (
                                            // based on XSD or else)
 
  private static int verbosityLevel=0;      // Higher numbers show more messages.
                                            // In addition, from level 2, file name
					    //    and line numbers appear.
  private static boolean highVerbosity=false; // true when verbosityLevel >= 4

  public static int max_threads=100;
  public static boolean send_stale_if_error=true;
  public static int validate_last_modified_seconds=-1;
  public static String last_modified_table_name;
  // Note that when the DB is down, at least on SLC4 it takes about
  //  6-1/3rd minutes for DataSource.getConnection() to return
  public static int max_db_acquire_seconds=300;
  public static int max_db_execute_seconds=10;

  public long max_age=-1;
  public long error_max_age=Frontier.errorDefaultMaxAge();
  public boolean noCache=false;
  public int payloads_num;
  public ArrayList<Payload> aPayloads=null;
  public long if_modified_since=-1;


  public static int getVerbosityLevel() 
   {
    return verbosityLevel;
   }

  public static boolean getHighVerbosity() 
   {
    return highVerbosity;
   }

  private static String getPropertyString(ResourceBundle rb,String name) throws Exception
   {
    try
     {
      String p=rb.getString(name);
      return p;
     }
    catch(MissingResourceException e)
     {          
      return null;
     }
   }
  
  
  protected static synchronized void init() throws Exception
   {
    if(initialized) return;
    ResourceBundle prb=PropertyResourceBundle.getBundle("config");
    conf_server_name=prb.getString("ServerName");
    conf_ds_name=prb.getString("DataSourceName");
        
    if(conf_server_name==null) throw new Exception("ServerName is missing in FrontierConfig");
    if(conf_ds_name==null) throw new Exception("DataSourceName is missing in FrontierConfig");

    Frontier.Log("Initializing Frontier servlet version "+FrontierServlet.frontierVersion());

    conf_xsd_table=getPropertyString(prb,"XsdTableName");

    // Verbosity level related
    String verbosity=getPropertyString(prb,"VerbosityLevel");
    if (verbosity!=null)
      verbosityLevel=Integer.parseInt(verbosity);
    highVerbosity=(verbosityLevel>=4);
    if (verbosityLevel>0)
      Frontier.Log("VerbosityLevel set to "+verbosityLevel);

    String maxthreads=getPropertyString(prb,"MaxThreads");
    if(maxthreads!=null)
      max_threads=Integer.parseInt(maxthreads);
    Frontier.Log("max threads: "+max_threads);

    String sendstale=getPropertyString(prb,"SendStaleIfError");
    if(sendstale!=null)
      send_stale_if_error=Boolean.parseBoolean(sendstale);
    Frontier.Log("send stale if error: "+send_stale_if_error);

    String maxsecs=getPropertyString(prb,"MaxDbAcquireSeconds");
    if(maxsecs!=null)
     {
      max_db_acquire_seconds=Integer.parseInt(maxsecs);
      Frontier.Log("max DB acquire secs: "+max_db_acquire_seconds);
     }
    maxsecs=getPropertyString(prb,"MaxDbExecuteSeconds");
    if(maxsecs!=null)
     {
      max_db_execute_seconds=Integer.parseInt(maxsecs);
      Frontier.Log("max DB execute secs: "+max_db_execute_seconds);
     }

    String str=getPropertyString(prb,"ValidateLastModifiedSeconds");
    if(str!=null)
     {
      validate_last_modified_seconds=Integer.parseInt(str);
      Frontier.Log("validate last-modified secs: "+validate_last_modified_seconds);
      last_modified_table_name=getPropertyString(prb,"LastModifiedTableName");
      if(last_modified_table_name==null)
        throw new Exception("LastModifiedTableName (required by ValidateLastModifiedSeconds) is missing in FrontierConfig");
      Frontier.Log("last-modified table name: "+last_modified_table_name);
     }

    conf_cache_expire_hourofday[LONGCACHE]=getPropertyString(prb,"LongCacheExpireHourOfDay");
    conf_cache_expire_seconds[LONGCACHE]=getPropertyString(prb,"LongCacheExpireSeconds");
    conf_cache_expire_hourofday[SHORTCACHE]=getPropertyString(prb,"ShortCacheExpireHourOfDay");
    conf_cache_expire_seconds[SHORTCACHE]=getPropertyString(prb,"ShortCacheExpireSeconds");

    // If no short time set, default it to 0 seconds
    if((conf_cache_expire_hourofday[SHORTCACHE]==null)&&
       (conf_cache_expire_seconds[SHORTCACHE]==null))
      conf_cache_expire_seconds[SHORTCACHE]="0";

    Frontier.Log("long hour: "+conf_cache_expire_hourofday[LONGCACHE]+", long secs: "+conf_cache_expire_seconds[LONGCACHE]);
    Frontier.Log("short hour: "+conf_cache_expire_hourofday[SHORTCACHE]+", short secs: "+conf_cache_expire_seconds[SHORTCACHE]);

    conf_monitor_node=getPropertyString(prb,"MonitorNode");
    conf_monitor_delay=getPropertyString(prb,"MonitorMillisDelay");
    if(conf_monitor_node!=null && conf_monitor_delay!=null) 
     {
      try
       {
        monitor=new Monitor(conf_monitor_node,conf_monitor_delay);
        monitor.setDaemon(true);
        monitor.start();
       }
      catch(Exception e)
       {
        Frontier.Log("MonAlisa monitor failed to start:",e);
        monitor=null;
       }
     } 
    else Frontier.Log("MonAlisa monitor was not configured.");
    
    String tmp=getPropertyString(prb,"UseFdoCache");
    use_fdo_cache=(tmp!=null && tmp.equalsIgnoreCase("yes"));
    if(use_fdo_cache) Frontier.Log("FDO cache is in use");
    else Frontier.Log("FDO cache is DISABLED");
        
    initialized=true;
   }
          

  public Frontier(HttpServletRequest req) throws Exception 
   {
    if(!initialized) init();

    DbConnectionMgr connMgr=null;
    ArrayList<Command> commandList=null;
    
    if(monitor!=null) monitor.increment();

    logClientDesc(req);          
    connMgr=DbConnectionMgr.getDbConnectionMgr();
    commandList=Command.parse(req);
    payloads_num=commandList.size();
    aPayloads=new ArrayList<Payload>();
    if(validate_last_modified_seconds>0)
     {
      if_modified_since=req.getDateHeader("if-modified-since");
      if(if_modified_since!=-1)
	Frontier.Log("if-modified-since: "+req.getHeader("if-modified-since"));
     }
    if(conf_cache_expire_seconds[SHORTCACHE]!=null)
     {
      //if set and less than the default error max age, use the short cache
      //  short cache age also for errors
      long age=Long.parseLong(conf_cache_expire_seconds[SHORTCACHE]);
      if(age<error_max_age)
        error_max_age=age;
     }
    int cachelength=LONGCACHE;
    for(int i=0;i<payloads_num;i++)
     {
      Command cmd=(Command)commandList.get(i);
      Payload p=new Payload(cmd,connMgr);
      if(p.noCache) noCache=true;
      String ttl=cmd.fds.getOptionalString("ttl");
      if((ttl!=null)&&(ttl.equals("short"))) cachelength=SHORTCACHE;
      long p_max_age=p.time_expire/1000;
      if(max_age<0) max_age=p_max_age;
      if(p_max_age<max_age) max_age=p_max_age;
      aPayloads.add(p);
     }
    if(conf_cache_expire_seconds[cachelength]!=null)
     {
      //if set, use configured expiration seconds for requested cache length
      //  instead of Payload's
      max_age=Long.parseLong(conf_cache_expire_seconds[cachelength]);
     }
    if(conf_cache_expire_hourofday[cachelength]!=null)
     {
      Calendar cal=Calendar.getInstance();
      long now=cal.getTimeInMillis();
      //use expiration hour-of-day if it is set and sooner
      int hourofday=Integer.parseInt(conf_cache_expire_hourofday[cachelength]);
      if(hourofday<=cal.get(Calendar.HOUR_OF_DAY))
	cal.add(Calendar.DAY_OF_YEAR,1);
      cal.set(Calendar.HOUR_OF_DAY,hourofday);
      cal.set(Calendar.MINUTE,0);
      cal.set(Calendar.SECOND,0);
      long diff=(cal.getTimeInMillis()-now)/1000;
      if(diff<max_age)
	max_age=diff;
     }
    //Frontier.Log("max-age="+max_age);
   }

  public static long errorDefaultMaxAge()
   {
    return 5*60;	// errors expire in 5 minutes
   }
   
  public long cachedLastModified() throws Exception
   {
    if(highVerbosity)Frontier.Log("cachedLastModified()");
    if(validate_last_modified_seconds<=0)
      return -1;
    long last_modified=0;
    // look for max last_modified of cachedLastModified() of each payload
    for(int i=0;i<payloads_num;i++)
     {
      Payload p=(Payload)aPayloads.get(i);          
      long lm=p.cachedLastModified();
      if(lm==-1)
        return -1;
      if(lm>last_modified)
        last_modified=lm;
     }
    if(highVerbosity)Frontier.Log("cachedLastModified(): last_modified: "+last_modified);
    return last_modified;
   }

  public long getLastModified(ServletOutputStream out) throws Exception
   {
    if(highVerbosity)Frontier.Log("Frontier.java:getLastModified()");
    long last_modified=0;
    for(int i=0;i<payloads_num;i++)
     {
      Payload p=(Payload)aPayloads.get(i);          
      if(highVerbosity)Frontier.Log("Frontier.java:getLastModified(): payload: "+i);
      long lm=p.getLastModified(out);
      if(lm>last_modified)
        last_modified=lm;
     }
    return last_modified;
   }

  public void close(ServletOutputStream sos) throws Exception
   {
    for(int i=0;i<payloads_num;i++)
     {
      Payload p=(Payload)aPayloads.get(i);          
      p.close(sos);
     }
   }

  private void logClientDesc(HttpServletRequest req) throws Exception
   {
    String qs;
    if((qs=req.getQueryString())==null)
     {
      if((qs=req.getPathInfo())==null)
        throw new Exception("no query");
     }
    String queryString=java.net.URLDecoder.decode(qs,"US-ASCII"); 
    StringBuffer client_desc=new StringBuffer("");
    client_desc.append("servlet_version:");
    client_desc.append(FrontierServlet.frontierVersion());
    client_desc.append(" start threads:");
    client_desc.append(FrontierServlet.numCurrentThreads());
    client_desc.append(" query ");
    client_desc.append(queryString);
    client_desc.append(" raddr ");
    client_desc.append(req.getRemoteAddr());
    client_desc.append(" frontier-id: ");
    String frontierId=req.getHeader("x-frontier-id");
    if(frontierId==null)
      client_desc.append("missing");
    else
      client_desc.append(frontierId);
    for(Enumeration en=req.getHeaderNames();en.hasMoreElements();)
     {
      String name=(String)en.nextElement();
      if(name.compareToIgnoreCase("via")==0 || name.compareToIgnoreCase("x-forwarded-for")==0)
       {
        client_desc.append(' ');
        client_desc.append(name);
        client_desc.append(": ");
        client_desc.append(req.getHeader(name));
       }
     }            
    Log(client_desc.toString());
    if(frontierId==null)
      throw new Exception("X-frontier-id header missing");
   }

   public static String getLineInfo()
    {
     return getLineInfo(2);
    }

   public static String getLineInfo(int stackIndex)
    {
     StackTraceElement ste=new Throwable().getStackTrace()[stackIndex];
     return ste.getFileName()+" +"+ste.getLineNumber();
    }

  // synchronize it to be very sure that log messages don't get 
  //  interleaved and because a SimpleDateFormat instance can't be used
  //  by multiple threads at the same time
  public static synchronized void Log(String msg)
   {
    StringBuffer buf=new StringBuffer("");
    buf.append(conf_server_name);
    buf.append(' ');
    buf.append(date_fmt.format(new Date()));
    buf.append(' ');
    String id=Thread.currentThread().getName();
    int idx=id.indexOf("id=");
    if(idx>0)
      id=id.substring(idx); // remove part before id=
    buf.append(id);
    buf.append(' ');
    if(verbosityLevel>=2)
     {
      buf.append(getLineInfo(2));
      buf.append(' ');
     }
    buf.append(msg);
    System.out.println(buf.toString());
   }

  public static void Log(String msg,Throwable e)
   {     
    ByteArrayOutputStream baos=new ByteArrayOutputStream();
    PrintWriter pw=new PrintWriter(baos);
    e.printStackTrace(pw);
    pw.flush();
    Log(msg+"\n"+baos.toString());
   }
     
    
  public static String getDsName()
   {
    return conf_ds_name;
   }
  
  public static String getServerName()
   {
    return conf_server_name;
   }
  
  
  public static String getXsdTableName()
   {
    return conf_xsd_table;
   }
   
  public static boolean isFdoCache()
   {
    return use_fdo_cache;
   }

  public static int getMaxDbAcquireSeconds()
   {
    return max_db_acquire_seconds;
   }

  public static int getMaxDbExecuteSeconds()
   {
    return max_db_execute_seconds;
   }

 } // class Frontier
