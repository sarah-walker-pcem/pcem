#define CALL_FAR_w(new_seg, new_pc)                                             \
        old_cs = CS;                                                            \
        old_pc = pc;                                                            \
        if (ssegs) ss = oldss;                                                  \
        oxpc = pc;                                                              \
        pc = new_pc;                                                            \
        optype = CALL;                                                          \
        cgate16 = cgate32 = 0;                                                  \
        if (msw & 1) loadcscall(new_seg);                                       \
        else         loadcs(new_seg);                                           \
        optype = 0;                                                             \
        if (abrt) { cgate16 = cgate32 = 0; return 0; }                          \
        oldss = ss;                                                             \
        if (cgate32)                                                            \
        {                                                                       \
                uint32_t old_esp = ESP;                                         \
                PUSH_L(old_cs);                         if (abrt) { cgate16 = cgate32 = 0; return 0; }     \
                PUSH_L(old_pc);                         if (abrt) ESP = old_esp; \
        }                                                                       \
        else                                                                    \
        {                                                                       \
                uint32_t old_esp = ESP;                                         \
                PUSH_W(old_cs);                         if (abrt) { cgate16 = cgate32 = 0; return 0; }     \
                PUSH_W(old_pc);                         if (abrt) ESP = old_esp; \
        }
        
#define CALL_FAR_l(new_seg, new_pc)                                             \
        old_cs = CS;                                                            \
        old_pc = pc;                                                            \
        if (ssegs) ss = oldss;                                                  \
        oxpc = pc;                                                              \
        pc = new_pc;                                                            \
        optype = CALL;                                                          \
        cgate16 = cgate32 = 0;                                                  \
        if (msw & 1) loadcscall(new_seg);                                       \
        else         loadcs(new_seg);                                           \
        optype = 0;                                                             \
        if (abrt) { cgate16 = cgate32 = 0; return 0; }                          \
        oldss = ss;                                                             \
        if (cgate16)                                                            \
        {                                                                       \
                uint32_t old_esp = ESP;                                         \
                PUSH_W(old_cs);                         if (abrt) { cgate16 = cgate32 = 0; return 0; }     \
                PUSH_W(old_pc);                         if (abrt) ESP = old_esp; \
        }                                                                       \
        else                                                                    \
        {                                                                       \
                uint32_t old_esp = ESP;                                         \
                PUSH_L(old_cs);                         if (abrt) { cgate16 = cgate32 = 0; return 0; }     \
                PUSH_L(old_pc);                         if (abrt) ESP = old_esp; \
        }
        

#define CALL_FAR_nr(new_seg, new_pc)                                               \
        old_cs = CS;                                                            \
        old_pc = pc;                                                            \
        if (ssegs) ss = oldss;                                                  \
        oxpc = pc;                                                              \
        pc = new_pc;                                                            \
        optype = CALL;                                                          \
        cgate16 = cgate32 = 0;                                                  \
        if (msw & 1) loadcscall(new_seg);                                       \
        else         loadcs(new_seg);                                           \
        optype = 0;                                                             \
        if (abrt) { cgate16 = cgate32 = 0; break; }                             \
        oldss = ss;                                                             \
        if (cgate32)                                                            \
        {                                                                       \
                uint32_t old_esp = ESP;                                         \
                PUSH_L(old_cs);                         if (abrt) { cgate16 = cgate32 = 0; break; }     \
                PUSH_L(old_pc);                         if (abrt) ESP = old_esp; \
        }                                                                       \
        else                                                                    \
        {                                                                       \
                uint32_t old_esp = ESP;                                         \
                PUSH_W(old_cs);                         if (abrt) { cgate16 = cgate32 = 0; break; }     \
                PUSH_W(old_pc);                         if (abrt) ESP = old_esp; \
        }                                                                       \
        
static int opCALL_far_w(uint32_t fetchdat)
{
        uint32_t old_cs, old_pc;
        uint16_t new_cs, new_pc;
        
        new_pc = getwordf();
        new_cs = getword();                             if (abrt) return 0;
        
        CALL_FAR_w(new_cs, new_pc);
        
        cycles -= is486 ? 18 : 17;
        return 0;
}
static int opCALL_far_l(uint32_t fetchdat)
{
        uint32_t old_cs, old_pc;
        uint32_t new_cs, new_pc;
        
        new_pc = getlong();
        new_cs = getword();                             if (abrt) return 0;
        
        CALL_FAR_l(new_cs, new_pc);
        
        cycles -= is486 ? 18 : 17;
        return 0;
}


static int opFF_w_a16(uint32_t fetchdat)
{
        uint16_t old_cs, new_cs;
        uint32_t old_pc, new_pc;
        
        uint16_t temp;
        
        fetch_ea_16(fetchdat);
        
        switch (rmdat & 0x38)
        {
                case 0x00: /*INC w*/
                temp = geteaw();                        if (abrt) return 0;
                seteaw(temp + 1);                       if (abrt) return 0;
                setadd16nc(temp, 1);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x08: /*DEC w*/
                temp = geteaw();                        if (abrt) return 0;
                seteaw(temp - 1);                       if (abrt) return 0;
                setsub16nc(temp, 1);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x10: /*CALL*/
                new_pc = geteaw();                      if (abrt) return 0;
                if (ssegs) ss = oldss;
                PUSH_W(pc);
                pc = new_pc;
                if (is486) cycles -= 5;
                else       cycles -= ((mod == 3) ? 7 : 10);
                break;
                case 0x18: /*CALL far*/
                new_pc = readmemw(easeg, eaaddr);
                new_cs = readmemw(easeg, (eaaddr + 2)); if (abrt) return 0;
                
                CALL_FAR_w(new_cs, new_pc);
                
                cycles -= is486 ? 17 : 22;
                break;
                case 0x20: /*JMP*/
                new_pc = geteaw();                      if (abrt) return 0;
                pc = new_pc;
                if (is486) cycles -= 5;
                else       cycles -= ((mod == 3) ? 7 : 10);
                break;
                case 0x28: /*JMP far*/
                oxpc = pc;
                new_pc = readmemw(easeg, eaaddr);
                new_cs = readmemw(easeg, eaaddr + 2);  if (abrt) return 0;
                pc = new_pc;
                loadcsjmp(new_cs, oxpc);               if (abrt) return 0;
                cycles -= is486 ? 13 : 12;
                break;
                case 0x30: /*PUSH w*/
                temp = geteaw();                        if (abrt) return 0;
                if (ssegs) ss = oldss;
                PUSH_W(temp);
                cycles -= ((mod == 3) ? 2 : 5);
                break;

                default:
                fatal("Bad FF opcode %02X\n",rmdat&0x38);
                x86illegal();
        }
        return 0;
}
static int opFF_w_a32(uint32_t fetchdat)
{
        uint16_t old_cs, new_cs;
        uint32_t old_pc, new_pc;
        
        uint16_t temp;
        
        fetch_ea_32(fetchdat);
        
        switch (rmdat & 0x38)
        {
                case 0x00: /*INC w*/
                temp = geteaw();                        if (abrt) return 0;
                seteaw(temp + 1);                       if (abrt) return 0;
                setadd16nc(temp, 1);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x08: /*DEC w*/
                temp = geteaw();                        if (abrt) return 0;
                seteaw(temp - 1);                       if (abrt) return 0;
                setsub16nc(temp, 1);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x10: /*CALL*/
                new_pc = geteaw();                      if (abrt) return 0;
                if (ssegs) ss = oldss;
                PUSH_W(pc);
                pc = new_pc;
                if (is486) cycles -= 5;
                else       cycles -= ((mod == 3) ? 7 : 10);
                break;
                case 0x18: /*CALL far*/
                new_pc = readmemw(easeg, eaaddr);
                new_cs = readmemw(easeg, (eaaddr + 2)); if (abrt) return 0;
                
                CALL_FAR_w(new_cs, new_pc);

                cycles -= is486 ? 17 : 22;
                break;
                case 0x20: /*JMP*/
                new_pc = geteaw();                      if (abrt) return 0;
                pc = new_pc;
                if (is486) cycles -= 5;
                else       cycles -= ((mod == 3) ? 7 : 10);
                break;
                case 0x28: /*JMP far*/
                oxpc = pc;
                new_pc = readmemw(easeg, eaaddr);
                new_cs = readmemw(easeg, eaaddr + 2);  if (abrt) return 0;
                pc = new_pc;
                loadcsjmp(new_cs, oxpc);               if (abrt) return 0;
                cycles -= is486 ? 13 : 12;
                break;
                case 0x30: /*PUSH w*/
                temp = geteaw();                        if (abrt) return 0;
                if (ssegs) ss = oldss;
                PUSH_W(temp);
                cycles -= ((mod == 3) ? 2 : 5);
                break;

                default:
                fatal("Bad FF opcode %02X\n",rmdat&0x38);
                x86illegal();
        }
        return 0;
}

static int opFF_l_a16(uint32_t fetchdat)
{
        uint16_t old_cs, new_cs;
        uint32_t old_pc, new_pc;
        
        uint32_t temp;
        
        fetch_ea_16(fetchdat);
        
        switch (rmdat & 0x38)
        {
                case 0x00: /*INC l*/
                temp = geteal();                        if (abrt) return 0;
                seteal(temp + 1);                       if (abrt) return 0;
                setadd32nc(temp, 1);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x08: /*DEC l*/
                temp = geteal();                        if (abrt) return 0;
                seteal(temp - 1);                       if (abrt) return 0;
                setsub32nc(temp, 1);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x10: /*CALL*/
                new_pc = geteal();                      if (abrt) return 0;
                if (ssegs) ss = oldss;
                PUSH_L(pc);
                pc = new_pc;
                if (is486) cycles -= 5;
                else       cycles -= ((mod == 3) ? 7 : 10);
                break;
                case 0x18: /*CALL far*/
                new_pc = readmeml(easeg, eaaddr);
                new_cs = readmemw(easeg, (eaaddr + 4)); if (abrt) return 0;
                
                CALL_FAR_l(new_cs, new_pc);
                
                cycles -= is486 ? 17 : 22;
                break;
                case 0x20: /*JMP*/
                new_pc = geteal();                      if (abrt) return 0;
                pc = new_pc;
                if (is486) cycles -= 5;
                else       cycles -= ((mod == 3) ? 7 : 10);
                break;
                case 0x28: /*JMP far*/
                oxpc = pc;
                new_pc = readmeml(easeg, eaaddr);
                new_cs = readmemw(easeg, eaaddr + 4);   if (abrt) return 0;
                pc = new_pc;
                loadcsjmp(new_cs, oxpc);                if (abrt) return 0;
                cycles -= is486 ? 13 : 12;
                break;
                case 0x30: /*PUSH l*/
                temp = geteal();                        if (abrt) return 0;
                if (ssegs) ss = oldss;
                PUSH_L(temp);
                cycles -= ((mod == 3) ? 2 : 5);
                break;

                default:
                fatal("Bad FF opcode %02X\n",rmdat&0x38);
                x86illegal();
        }
        return 0;
}
static int opFF_l_a32(uint32_t fetchdat)
{
        uint16_t old_cs, new_cs;
        uint32_t old_pc, new_pc;
        
        uint32_t temp;
        
        fetch_ea_32(fetchdat);
        
        switch (rmdat & 0x38)
        {
                case 0x00: /*INC l*/
                temp = geteal();                        if (abrt) return 0;
                seteal(temp + 1);                       if (abrt) return 0;
                setadd32nc(temp, 1);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x08: /*DEC l*/
                temp = geteal();                        if (abrt) return 0;
                seteal(temp - 1);                       if (abrt) return 0;
                setsub32nc(temp, 1);
                cycles -= (mod == 3) ? timing_rr : timing_mm;
                break;
                case 0x10: /*CALL*/
                new_pc = geteal();                      if (abrt) return 0;
                if (ssegs) ss = oldss;
                PUSH_L(pc);                             if (abrt) return 0;
                pc = new_pc;
                if (is486) cycles -= 5;
                else       cycles -= ((mod == 3) ? 7 : 10);
                break;
                case 0x18: /*CALL far*/
                new_pc = readmeml(easeg, eaaddr);
                new_cs = readmemw(easeg, (eaaddr + 4)); if (abrt) return 0;
                
                CALL_FAR_l(new_cs, new_pc);
                
                cycles -= is486 ? 17 : 22;
                break;
                case 0x20: /*JMP*/
                new_pc = geteal();                      if (abrt) return 0;
                pc = new_pc;
                if (is486) cycles -= 5;
                else       cycles -= ((mod == 3) ? 7 : 10);
                break;
                case 0x28: /*JMP far*/
                oxpc = pc;
                new_pc = readmeml(easeg, eaaddr);
                new_cs = readmemw(easeg, eaaddr + 4);   if (abrt) return 0;
                pc = new_pc;
                loadcsjmp(new_cs, oxpc);                if (abrt) return 0;
                cycles -= is486 ? 13 : 12;
                break;
                case 0x30: /*PUSH l*/
                temp = geteal();                        if (abrt) return 0;
                if (ssegs) ss = oldss;
                PUSH_L(temp);
                cycles -= ((mod == 3) ? 2 : 5);
                break;

                default:
                fatal("Bad FF opcode %02X\n",rmdat&0x38);
                x86illegal();
        }
        return 0;
}
