#ifndef _T1000_H_
#define _T1000_H_
void t1000_syskey(uint8_t andmask, uint8_t ormask, uint8_t xormask);

void t1000_configsys_loadnvr();
void t1000_emsboard_loadnvr();
void t1200_state_loadnvr();

void t1000_configsys_savenvr();
void t1000_emsboard_savenvr();
void t1200_state_savenvr();


#endif /* _T1000_H_ */
