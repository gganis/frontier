/*
 * FroNTier client API
 * 
 * Author: Sergey Kosyakov
 *
 * $Header$
 *
 * $Id$
 *
 */

#include <frontier-cpp.h>
#include <sstream>

using namespace frontier;

static const char *blob_type_name[]={"byte","int4","int8","float","double","time","string"};
 
int AnyData::castToInt()
 {
  frontier_log(FRONTIER_LOGLEVEL_WARNING,__FILE__,__LINE__,"converting %s to int",blob_type_name[t]);
  switch(t)
   {
    case BLOB_TYPE_BYTE: return (int)v.b;
    case BLOB_TYPE_TIME:
    case BLOB_TYPE_INT8: return (int)v.i8;
    case BLOB_TYPE_FLOAT: return (int)v.f;
    case BLOB_TYPE_DOUBLE: return (int)v.d;
    case BLOB_TYPE_ARRAY_BYTE: return atoi(v.str.p);
    default: break; // Just to make this geek gcc happy
   }
  type_error=FRONTIER_EUNKNOWN;
  frontier_setErrorMsg(__FILE__,__LINE__,"something wrong out there");
  return 0;
 }
 
long long AnyData::castToLongLong()
 {
  frontier_log(FRONTIER_LOGLEVEL_WARNING,__FILE__,__LINE__,"converting %s to long long",blob_type_name[t]);
  switch(t)
   {
    case BLOB_TYPE_BYTE: return (long long)v.b;
    case BLOB_TYPE_INT4: return (long long)v.i4;
    case BLOB_TYPE_FLOAT: return (long long)v.f;
    case BLOB_TYPE_DOUBLE: return (long long)v.d;
    case BLOB_TYPE_ARRAY_BYTE: return atoll(v.str.p);
    default: break; // Just to make this geek gcc happy
   }
  type_error=FRONTIER_EUNKNOWN;
  frontier_setErrorMsg(__FILE__,__LINE__,"something wrong out there");
  return 0;
 }
   
float AnyData::castToFloat()
 {
  frontier_log(FRONTIER_LOGLEVEL_WARNING,__FILE__,__LINE__,"converting %s to float",blob_type_name[t]);
  switch(t)
   {
    case BLOB_TYPE_BYTE: return (float)v.b;
    case BLOB_TYPE_INT4: return (float)v.i4;
    case BLOB_TYPE_TIME:
    case BLOB_TYPE_INT8: return (float)v.i8;    
    case BLOB_TYPE_DOUBLE: return (float)v.d;
    case BLOB_TYPE_ARRAY_BYTE: return (float)atof(v.str.p);
    default: break; // Just to make this geek gcc happy
   }
  type_error=FRONTIER_EUNKNOWN;
  frontier_setErrorMsg(__FILE__,__LINE__,"something wrong out there");
  return 0;
 }
   
double AnyData::castToDouble()
 {
  frontier_log(FRONTIER_LOGLEVEL_WARNING,__FILE__,__LINE__,"converting %s to double",blob_type_name[t]);
  switch(t)
   {
    case BLOB_TYPE_BYTE: return (double)v.b;
    case BLOB_TYPE_INT4: return (double)v.i4;
    case BLOB_TYPE_TIME:
    case BLOB_TYPE_INT8: return (double)v.i8;    
    case BLOB_TYPE_FLOAT: return (double)v.f;
    case BLOB_TYPE_ARRAY_BYTE: return atof(v.str.p);
    default: break; // Just to make this geek gcc happy
   }
  type_error=FRONTIER_EUNKNOWN;
  frontier_setErrorMsg(__FILE__,__LINE__,"something wrong out there");
  return 0;
 }
 
std::string* AnyData::castToString()
 {
  frontier_log(FRONTIER_LOGLEVEL_WARNING,__FILE__,__LINE__,"converting %s to string",blob_type_name[t]);
  std::ostringstream oss;
  switch(t)
   {
    case BLOB_TYPE_BYTE: oss<<v.b; return new std::string(oss.str());
    case BLOB_TYPE_INT4: oss<<v.i4; return new std::string(oss.str());
    case BLOB_TYPE_TIME:
    case BLOB_TYPE_INT8: oss<<v.i8; return new std::string(oss.str());
    case BLOB_TYPE_FLOAT: oss<<v.f; return new std::string(oss.str());
    case BLOB_TYPE_DOUBLE: oss<<v.d; return new std::string(oss.str());
    default: break; // Just to make this geek gcc happy
   }
  type_error=FRONTIER_EUNKNOWN;
  frontier_setErrorMsg(__FILE__,__LINE__,"something wrong out there");
  return NULL;
 }


