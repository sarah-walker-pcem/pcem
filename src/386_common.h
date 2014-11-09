extern uint16_t ea_rseg;

#undef readmemb
#undef writememb


#define readmemb(s,a) ((readlookup2[((s)+(a))>>12]==-1 || (s)==0xFFFFFFFF)?readmemb386l(s,a): *(uint8_t *)(readlookup2[((s)+(a))>>12] + (s) + (a)) )

#define writememb(s,a,v) if (writelookup2[((s)+(a))>>12]==-1 || (s)==0xFFFFFFFF) writememb386l(s,a,v); else *(uint8_t *)(writelookup2[((s) + (a)) >> 12] + (s) + (a)) = v

#define writememw(s,a,v) if (writelookup2[((s)+(a))>>12]==-1 || (s)==0xFFFFFFFF || (((s)+(a))&0xFFF)>0xFFE) writememwl(s,a,v); else *(uint16_t *)(writelookup2[((s) + (a)) >> 12] + (s) + (a)) = v
#define writememl(s,a,v) if (writelookup2[((s)+(a))>>12]==-1 || (s)==0xFFFFFFFF || (((s)+(a))&0xFFF)>0xFFC) writememll(s,a,v); else *(uint32_t *)(writelookup2[((s) + (a)) >> 12] + (s) + (a)) = v


#define check_io_perm(port) if (!IOPLp || (eflags&VM_FLAG)) \
                        { \
                                int tempi = checkio(port); \
                                if (abrt) return 1; \
                                if (tempi) \
                                { \
                                        x86gpf(NULL,0); \
                                        return 1; \
                                } \
                        }

#define checkio_perm(port) if (!IOPLp || (eflags&VM_FLAG)) \
                        { \
                                tempi = checkio(port); \
                                if (abrt) break; \
                                if (tempi) \
                                { \
                                        x86gpf(NULL,0); \
                                        break; \
                                } \
                        }

#define CHECK_READ(seg, low, high)  \
        if ((low < (seg)->limit_low) || (high > (seg)->limit_high))       \
        {                                       \
                x86gpf("Limit check", 0);       \
                return 1;                       \
        }

#define CHECK_WRITE(seg, low, high)  \
        if ((low < (seg)->limit_low) || (high > (seg)->limit_high))       \
        {                                       \
                x86gpf("Limit check", 0);       \
                return 1;                       \
        }

#define CHECK_WRITE_REP(seg, low, high)  \
        if ((low < (seg)->limit_low) || (high > (seg)->limit_high))       \
        {                                       \
                x86gpf("Limit check", 0);       \
                break;                       \
        }


#define NOTRM   if (!(msw & 1) || (eflags & VM_FLAG))\
                { \
                        x86_int(6); \
                        return 1; \
                }




static inline uint8_t fastreadb(uint32_t a)
{
        if ((a >> 12) == pccache) 
                return *((uint8_t *)&pccache2[a]);
        pccache2 = getpccache(a);
        if (abrt)
                return;
        pccache = a >> 12;
        return *((uint8_t *)&pccache2[a]);
}

static inline uint16_t fastreadw(uint32_t a)
{
        uint32_t t;
        if ((a&0xFFF)>0xFFE)
        {
                t=readmemb(0,a);
                t|=(readmemb(0,a+1)<<8);
                return t;
        }
        if ((a>>12)==pccache) return *((uint16_t *)&pccache2[a]);
        pccache2=getpccache(a);
        if (abrt)
                return;
        pccache=a>>12;
        return *((uint16_t *)&pccache2[a]);
}

static inline uint32_t fastreadl(uint32_t a)
{
        uint8_t *t;
        uint32_t val;
        if ((a&0xFFF)<0xFFD)
        {
                if ((a>>12)!=pccache)
                {
                        t = getpccache(a);
                        if (abrt) return 0;
                        pccache2 = t;
                        pccache=a>>12;
                        //return *((uint32_t *)&pccache2[a]);
                }
                return *((uint32_t *)&pccache2[a]);
        }
        val  =readmemb(0,a);
        val |=(readmemb(0,a+1)<<8);
        val |=(readmemb(0,a+2)<<16);
        val |=(readmemb(0,a+3)<<24);
        return val;
}

static inline uint8_t getbyte()
{
        pc++;
        return fastreadb(cs + (pc - 1));
}

static inline uint16_t getword()
{
        pc+=2;
        return fastreadw(cs+(pc-2));
}

static inline uint32_t getlong()
{
        pc+=4;
        return fastreadl(cs+(pc-4));
}

static inline uint64_t getquad()
{
        pc+=8;
        return fastreadl(cs+(pc-8)) | ((uint64_t)fastreadl(cs+(pc-4)) << 32);
}



static inline uint8_t geteab()
{
        if (mod==3)
           return (rm&4)?regs[rm&3].b.h:regs[rm&3].b.l;
        if (eal_r) return *(uint8_t *)eal_r;
        return readmemb(easeg,eaaddr);
}

static inline uint16_t geteaw()
{
        if (mod==3)
           return regs[rm].w;
//        cycles-=3;
        if (eal_r) return *(uint16_t *)eal_r;
        return readmemw(easeg,eaaddr);
}

static inline uint32_t geteal()
{
        if (mod==3)
           return regs[rm].l;
//        cycles-=3;
        if (eal_r) return *eal_r;
        return readmeml(easeg,eaaddr);
}

static inline uint64_t geteaq()
{
        return readmeml(easeg,eaaddr) | ((uint64_t)readmeml(easeg, eaaddr + 4) << 32);
}

static inline uint8_t geteab_mem()
{
        if (eal_r) return *(uint8_t *)eal_r;
        return readmemb(easeg,eaaddr);
}
static inline uint16_t geteaw_mem()
{
        if (eal_r) return *(uint16_t *)eal_r;
        return readmemw(easeg,eaaddr);
}
static inline uint32_t geteal_mem()
{
        if (eal_r) return *eal_r;
        return readmeml(easeg,eaaddr);
}

static inline void seteaq(uint64_t v)
{
        writememll(easeg, eaaddr, v & 0xffffffff); if (abrt) return;
        writememll(easeg, eaaddr + 4, v >> 32);
}

#define seteab(v) if (mod!=3) { if (eal_w) *(uint8_t *)eal_w=v;  else writememb386l(easeg,eaaddr,v); } else if (rm&4) regs[rm&3].b.h=v; else regs[rm].b.l=v
#define seteaw(v) if (mod!=3) { if (eal_w) *(uint16_t *)eal_w=v; else writememwl(easeg,eaaddr,v);    } else regs[rm].w=v
#define seteal(v) if (mod!=3) { if (eal_w) *eal_w=v;             else writememll(easeg,eaaddr,v);    } else regs[rm].l=v

#define seteab_mem(v) if (eal_w) *(uint8_t *)eal_w=v;  else writememb386l(easeg,eaaddr,v);
#define seteaw_mem(v) if (eal_w) *(uint16_t *)eal_w=v; else writememwl(easeg,eaaddr,v);
#define seteal_mem(v) if (eal_w) *eal_w=v;             else writememll(easeg,eaaddr,v);

#define getbytef() ((uint8_t)(fetchdat)); pc++
#define getwordf() ((uint16_t)(fetchdat)); pc+=2
#define getbyte2f() ((uint8_t)(fetchdat>>8)); pc++
#define getword2f() ((uint16_t)(fetchdat>>8)); pc+=2


#define rmdat rmdat32
#define fetchdat rmdat32
