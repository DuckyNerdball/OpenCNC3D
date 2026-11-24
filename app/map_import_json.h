/* Minimal bounded JSON reader for Remastered terrain sidecars. No dependency on
 * a Python install, and no regular-expression guessing of nested metadata. */
#pragma once
namespace mapimport { namespace detail {
struct JsonValue {
    char type = 0;
    std::string value;
    std::map<std::string, JsonValue> object;
    std::vector<JsonValue> array;
};
struct JsonReader {
    const std::string& text; size_t pos = 0; int nodes = 0;
    explicit JsonReader(const std::string& s):text(s) {}
    void space() { while(pos<text.size() && (text[pos]==' '||text[pos]=='\r'||text[pos]=='\n'||text[pos]=='\t'))++pos; }
    bool take(char c) { space(); if(pos==text.size()||text[pos]!=c)return false; ++pos; return true; }
    bool string(std::string& out) {
        if(!take('"'))return false;
        while(pos<text.size()) {
            unsigned char c=text[pos++]; if(c=='"')return true; if(c<32)return false;
            if(c!='\\'){out+=(char)c;continue;}
            if(pos==text.size())return false;c=text[pos++];
            if(c=='"'||c=='/'||c=='\\')out+=(char)c;
            else if(c=='b')out+='\b'; else if(c=='f')out+='\f'; else if(c=='n')out+='\n'; else if(c=='r')out+='\r'; else if(c=='t')out+='\t';
            else if(c=='u') {
                unsigned cp=0;for(int i=0;i<4;++i){if(pos==text.size())return false;char h=text[pos++];int n=h>='0'&&h<='9'?h-'0':h>='a'&&h<='f'?h-'a'+10:h>='A'&&h<='F'?h-'A'+10:-1;if(n<0)return false;cp=cp*16+n;}
                if(cp>=0xd800&&cp<=0xdfff)return false;
                if(cp<128)out+=(char)cp;else if(cp<2048){out+=(char)(192|(cp>>6));out+=(char)(128|(cp&63));}else{out+=(char)(224|(cp>>12));out+=(char)(128|((cp>>6)&63));out+=(char)(128|(cp&63));}
            } else return false;
        }return false;
    }
    bool read(JsonValue& v,int depth=0) {
        space(); if(depth>16||++nodes>100000||pos==text.size())return false;
        char c=text[pos];
        if(c=='{') {v.type='o';++pos;if(take('}'))return true;do{std::string k;JsonValue x;if(!string(k)||!take(':')||!read(x,depth+1)||v.object.count(k))return false;v.object[k]=x;if(take('}'))return true;}while(take(','));return false;}
        if(c=='[') {v.type='a';++pos;if(take(']'))return true;do{JsonValue x;if(!read(x,depth+1))return false;v.array.push_back(x);if(take(']'))return true;}while(take(','));return false;}
        if(c=='"'){v.type='s';return string(v.value);}
        if(c=='t'||c=='f'||c=='n'){std::string word=c=='t'?"true":c=='f'?"false":"null";if(text.compare(pos,word.size(),word))return false;pos+=word.size();v.type=c;return true;}
        size_t begin=pos;if(c=='-')++pos;if(pos==text.size())return false;
        if(text[pos]=='0')++pos;else{if(text[pos]<'1'||text[pos]>'9')return false;while(pos<text.size()&&isdigit((unsigned char)text[pos]))++pos;}
        if(pos<text.size()&&text[pos]=='.'){++pos;size_t at=pos;while(pos<text.size()&&isdigit((unsigned char)text[pos]))++pos;if(pos==at)return false;}
        if(pos<text.size()&&(text[pos]=='e'||text[pos]=='E')){++pos;if(pos<text.size()&&(text[pos]=='+'||text[pos]=='-'))++pos;size_t at=pos;while(pos<text.size()&&isdigit((unsigned char)text[pos]))++pos;if(pos==at)return false;}
        v.type='n';v.value=text.substr(begin,pos-begin);return true;
    }
};
inline bool recover_json(const std::string& bin, Bytes& source, Bytes& original, Info& out) {
    bool ambiguous=false;std::string path=sibling(bin,".json",ambiguous);
    if(ambiguous||path.empty())return fail(out,"This C&C terrain file needs a matching INI or Remastered JSON sidecar.");
    if(!read(path,original,out))return false;
    std::string s=text(original);JsonValue root;JsonReader parser(s);
    if(!parser.read(root)||root.type!='o')return fail(out,"The Remastered JSON sidecar is invalid.");parser.space();if(parser.pos!=s.size())return fail(out,"The JSON sidecar has trailing data.");
    int x,y,w,h;auto num=[&](const char* key,int& n){auto it=root.object.find(key);return it!=root.object.end()&&it->second.type=='n'&&integer(it->second.value,n);};
    auto theater=root.object.find("Theater"),starts=root.object.find("Waypoints");
    if(!num("MapTileX",x)||!num("MapTileY",y)||!num("MapTileWidth",w)||!num("MapTileHeight",h)||theater==root.object.end()||theater->second.type!='s'||starts==root.object.end()||starts->second.type!='a'||starts->second.array.size()<2||starts->second.array.size()>8)
        return fail(out,"The JSON sidecar lacks a playable rectangle, theater or multiplayer starts; a missing singleplayer mission cannot be reconstructed.");
    const std::string native_theater=upper(theater->second.value);
    if(native_theater!="TEMPERATE"&&native_theater!="TEMPERAT"&&native_theater!="DESERT"&&native_theater!="WINTER"&&native_theater!="SNOW"&&native_theater!="SAND")return fail(out,"The JSON sidecar has an unsupported theater.");
    // The JSON format has no mission scripts, objects or overlay. Only recover an
    // explicitly described terrain/start layout and tell the user what is absent.
    std::string name=stem(bin);for(char& c:name)if(c=='\r'||c=='\n')c=' ';
    std::string ini="[Basic]\nName="+name+"\nPlayer=Multi1\nSoloMission=False\n[Map]\nX="+std::to_string(x)+"\nY="+std::to_string(y)+"\nWidth="+std::to_string(w)+"\nHeight="+std::to_string(h)+"\nTheater="+theater->second.value+"\n";
    if(x+w>64||y+h>64)ini+="Version=1\n";
    ini+="[Waypoints]\n";int slot=0;
    for(const JsonValue& v:starts->second.array){int cell;if(v.type!='n'||!integer(v.value,cell)||cell<0)return fail(out,"The JSON sidecar has an invalid start cell.");ini+=std::to_string(slot++)+"="+std::to_string(cell)+"\n";}
    source=bytes(ini);out.recovered=true;out.warnings="Missing INI: recovered terrain-only skirmish from Remastered JSON. Original mission objects, resources and scripts are unavailable.";return true;
}
} }
