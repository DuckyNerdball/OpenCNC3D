#ifndef OPENCNC3D_OBJECT_DUMP_BUFFER_H
#define OPENCNC3D_OBJECT_DUMP_BUFFER_H
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
/* Optional synchronous text snapshot transport. Formatting and field order are
   shared with the diagnostic stdout dump. The caller owns a bounded byte buffer;
   no simulation state, FILE stream or process-wide stdout descriptor is changed. */
struct OpenCNC3D_Dump_Buffer {
    char* data;int capacity;int used;bool failed;
};
static OpenCNC3D_Dump_Buffer* OpenCNC3D_Dump_Output = NULL;
static void OpenCNC3D_Dump_Append(OpenCNC3D_Dump_Buffer& out,const char* data,size_t length) {
    if(out.failed)return;
    if(length>(size_t)(INT_MAX-out.used-1)){out.failed=true;return;}
    const int room=out.used<out.capacity?out.capacity-out.used:0;
    if(room>0) {
        const size_t copied=length<(size_t)room?length:(size_t)room-1;
        if(copied)memcpy(out.data+out.used,data,copied);
        out.data[out.used+(int)copied]=0;
    }
    out.used+=(int)length;
}
/* The snapshot schema uses only plain strings and signed decimal integers.
   Handling those directly avoids the general CRT formatter's character callbacks.
   Any future format outside that schema uses vsnprintf for the whole call. */
static int OpenCNC3D_Dump_Format(OpenCNC3D_Dump_Buffer& output,const char* format,va_list args) {
    for(const char* p=format;*p;p++)if(*p=='%') {
        p++;if(*p!='s'&&*p!='d'&&*p!='%') {
            const int room=output.used<output.capacity?output.capacity-output.used:0;
            const int result=vsnprintf(room?output.data+output.used:NULL,(size_t)room,format,args);
            if(result<0 || result>INT_MAX-output.used-1)output.failed=true;
            else output.used+=result;
            return result;
        }
    }
    const int before=output.used;
    for(const char* p=format;*p;) {
        const char* start=p;while(*p&&*p!='%')p++;
        OpenCNC3D_Dump_Append(output,start,(size_t)(p-start));
        if(!*p)break;
        p++;const char spec=*p++;
        if(spec=='s') {
            const char* value=va_arg(args,const char*);if(!value)value="(null)";
            OpenCNC3D_Dump_Append(output,value,strlen(value));
        } else if(spec=='d') {
            const int value=va_arg(args,int);unsigned magnitude=(unsigned)value;
            if(value<0)magnitude=0u-magnitude;
            char digits[12];char* end=digits+sizeof digits;char* at=end;
            do{*--at=(char)('0'+magnitude%10u);magnitude/=10u;}while(magnitude);
            if(value<0)*--at='-';
            OpenCNC3D_Dump_Append(output,at,(size_t)(end-at));
        } else OpenCNC3D_Dump_Append(output,"%",1);
    }
    return output.failed?-1:output.used-before;
}
static int OpenCNC3D_Dump_Printf(const char* format, ...) {
    va_list args;va_start(args,format);
    const int result=OpenCNC3D_Dump_Output?OpenCNC3D_Dump_Format(*OpenCNC3D_Dump_Output,format,args):vprintf(format,args);
    va_end(args);return result;
}
#endif
