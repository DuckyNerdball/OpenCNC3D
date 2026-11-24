#ifndef CNC_FIRE_VISIBILITY_H
#define CNC_FIRE_VISIBILITY_H
#include <map>
#include <utility>
/* Per-house transient firing reveals. Normal sight promotes touched cells to
   permanent exploration, including cells already temporarily marked mapped. */
struct CncFireVisibility {
    struct Cell { bool mapped,visible; int until; };
    typedef std::pair<int,int> Key; /* cell, house */
    std::map<Key,Cell> cells;
    bool temporary=false, nextTemporary=false, restoring=false;
    void touch(int cell,int house,bool mapped,bool visible,int frame) {
        if (restoring) return;
        const Key key(cell,house);
        if (!temporary) {if(!cells.empty())cells.erase(key);return;}
        auto it=cells.find(key);
        if (it!=cells.end()) {it->second.until=frame+45;return;}
        if (!mapped || !visible) cells[key]=Cell{mapped,visible,frame+45};
    }
    void clear() {cells.clear();temporary=nextTemporary=restoring=false;}
};
extern CncFireVisibility g_cncFireVisibility;
struct CncFireSightScope {
    bool previous;
    CncFireSightScope():previous(g_cncFireVisibility.temporary) {
        g_cncFireVisibility.temporary=g_cncFireVisibility.nextTemporary;
        g_cncFireVisibility.nextTemporary=false;
    }
    ~CncFireSightScope(){g_cncFireVisibility.temporary=previous;}
};
/* Implemented beside MapClass::Sight_From in each game. */
void CncFireReveal(int cell,int range,void* house);
void CncFireRevealTick();
void CncFireRevealRestore(bool clear);
void CncFireRevealResume();
/* Save payloads contain permanent exploration only; the running match retains
   its exact temporary state. Loading first removes the old transient marks. */
struct CncFireSaveScope {
    bool save;
    explicit CncFireSaveScope(bool saving):save(saving) {CncFireRevealRestore(!save);if(save)g_cncFireVisibility.restoring=true;}
    ~CncFireSaveScope(){if(save)CncFireRevealResume();}
};
#endif
