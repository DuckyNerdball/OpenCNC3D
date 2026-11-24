#ifndef OPENCNC3D_CAMPAIGN_PROGRESS_H
#define OPENCNC3D_CAMPAIGN_PROGRESS_H

/* TD StructType::STRUCT_AIRSTRIP is 11. The map row is advanced BEFORE
   presenting Map_Selection, as in the original scenario.cpp Do_Win. */
static int camp_map_row(int red_alert, int side, int completed, int sabotaged)
{
    return completed + (!red_alert && side == 0 && completed == 6 && sabotaged == 11);
}

#endif
