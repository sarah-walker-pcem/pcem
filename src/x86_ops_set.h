#define opSET(condition)                                                \
        static int opSET ## condition ## _a16(uint32_t fetchdat)        \
        {                                                               \
                fetch_ea_16(fetchdat);                                  \
                seteab((cond_ ## condition) ? 1 : 0);                   \
                cycles -= 4;                                            \
                return 0;                                               \
        }                                                               \
                                                                        \
        static int opSET ## condition ## _a32(uint32_t fetchdat)        \
        {                                                               \
                fetch_ea_32(fetchdat);                                  \
                seteab((cond_ ## condition) ? 1 : 0);                   \
                cycles -= 4;                                            \
                return 0;                                               \
        }

opSET(O)
opSET(NO)
opSET(B)
opSET(NB)
opSET(E)
opSET(NE)
opSET(BE)
opSET(NBE)
opSET(S)
opSET(NS)
opSET(P)
opSET(NP)
opSET(L)
opSET(NL)
opSET(LE)
opSET(NLE)
