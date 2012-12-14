#include "config.h"
#include "qemu-common.h"
#include "monitor.h"
#include "qemu-char.h"
#include "sysemu.h"
#include "acidstub.h"
#include "cpu.h"
#include "acidureg.h"

#ifndef TARGET_CPU_MEMORY_RW_DEBUG
static inline int target_memory_rw_debug(CPUArchState *env, target_ulong addr,
                                         uint8_t *buf, int len, int is_write)
{
    return cpu_memory_rw_debug(env, addr, buf, len, is_write);
}
#else
/* target_memory_rw_debug() defined in cpu.h */
#endif

// XXX: how can I find out what my target architecture is?
// XXX: I put my ureg.h in acidureg.h in the individual architectures?

typedef struct AcidState {
    CPUArchState *c_cpu; /* current CPU for step/continue ops */
    CPUArchState *g_cpu; /* current CPU for other ops */
    CPUArchState *query_cpu; /* for q{f|s}ThreadInfo */
//     enum RSState state; /* parsing state */
//     char line_buf[MAX_PACKET_LENGTH];
//     int line_buf_index;
//     int line_csum;
//     uint8_t last_packet[MAX_PACKET_LENGTH + 4];
//     int last_packet_len;
//     int signal;
    CharDriverState *chr;
    CharDriverState *mon_chr;
//     char syscall_buf[256];
//     gdb_syscall_complete_cb current_syscall_cb;
} AcidState;

static AcidState *acidserver_state;
// See plan9 explanation for how ureg works. 
static Ureg *ureg;

static void put_buffer(AcidState *s, const uint8_t *buf, int len)
{
    qemu_chr_fe_write(s->chr, buf, len);
}

/* Remote kernel debug protocol */
enum
{
    Terr='0',   /* not sent */
    Rerr,
    Tmget,
    Rmget,
    Tmput,
    Rmput,

    Tspid,  /* obsolete */
    Rspid,  /* obsolete */
    Tproc,
    Rproc,
    Tstatus,
    Rstatus,
    Trnote,
    Rrnote,

    Tstartstop,
    Rstartstop,
    Twaitstop,
    Rwaitstop,
    Tstart,
    Rstart,
    Tstop,
    Rstop,
    Tkill,
    Rkill,

    Tcondbreak,
    Rcondbreak,

    RDBMSGLEN = 10  /* tag byte, 9 data bytes */
};

#if defined(TARGET_I386)

/*
Whoo boy is this ugly. The Plan 9 compilers create regular structure layouts.
The libmach debugging library maps the Ureg (register and system state) structure to address zero.
Poking the registers is just a matter of reading and writing the registers.

To get the same effects in qemu we have an implicit ureg structure which maps to the 
internal structures of the CPU.*State structure of Qemu.

This initial 
*/

/*
This is 64 bit.
*/

#ifdef TARGET_X86_64
struct Uregmap {
    uint32_t uregoffset;
    uint32_t qemuoffset;
} ureg_map_init[] = {
    {offsetof(Ureg, ax), offsetof(CPUX86State, regs)+R_EAX},
    {offsetof(Ureg, bx), offsetof(CPUX86State, regs)+R_EBX},
    {offsetof(Ureg, cx), offsetof(CPUX86State, regs)+R_ECX},
    {offsetof(Ureg, dx), offsetof(CPUX86State, regs)+R_EDX},
    {offsetof(Ureg, si), offsetof(CPUX86State, regs)+R_ESI},
    {offsetof(Ureg, di), offsetof(CPUX86State, regs)+R_EDI},
    {offsetof(Ureg, bp), offsetof(CPUX86State, regs)+R_EBP},
    {offsetof(Ureg, sp), offsetof(CPUX86State, regs)+R_ESP},

    {offsetof(Ureg, ip), offsetof(CPUX86State, eip)},
// XXX
    {offsetof(Ureg, cs), offsetof(CPUX86State, regs)},
    {offsetof(Ureg, flags), offsetof(CPUX86State, eflags)},
    // XXX
    {offsetof(Ureg, ss), offsetof(CPUX86State, regs)},


};
#else
#endif


// XXX do a ken thompson and fill this array at setup.
//  This is goiing to end up a mess. 
void cpu_to_ureg(CPUX86State *s, Ureg *u);
void
cpu_to_ureg(CPUX86State *s, Ureg *u) {
// what you're really doing is making a translation table. 
    // you aren't going to be getting that much information all the time. 
    //  but I can make the rest of the stuff work. 

#ifdef TARGET_X86_64
    printf("u->ip offset %lx\n", offsetof(Ureg, ip));
    u->ax = (uint32_t)s->regs[R_EAX];
    printf("u->ax offset %lx\n", offsetof(Ureg, ax));
    u->bx = (uint32_t)s->regs[R_EBX];
    u->cx = (uint32_t)s->regs[R_ECX];
    u->dx = (uint32_t)s->regs[R_EDX];
    printf("u->dx value %lx\n", offsetof(Ureg, dx));
    u->si =(uint32_t)s->regs[R_ESI];
    u->di =(uint32_t)s->regs[R_EDI];
    u->bp = (uint32_t)s->regs[R_EBP];
    u->sp = (uint32_t)s->regs[R_ESP];
    u->ip = s->eip;
    printf("u->ip %lx s->eip %lx\n", u->ip, s->eip);
#else
    printf("u->pc offset %lx\n", offsetof(Ureg, pc));
    u->ax = (uint32_t)s->regs[R_EAX];
    printf("u->ax offset %lx\n", offsetof(Ureg, ax));
    u->bx = (uint32_t)s->regs[R_EBX];
    u->cx = (uint32_t)s->regs[R_ECX];
    u->dx = (uint32_t)s->regs[R_EDX];
    printf("u->dx value %lx\n", offsetof(Ureg, dx));
    u->si =(uint32_t)s->regs[R_ESI];
    u->di =(uint32_t)s->regs[R_EDI];
    u->bp = (uint32_t)s->regs[R_EBP];
    u->u0.sp = (uint32_t)s->regs[R_ESP];
    u->pc = s->eip;
    printf("u->pc %lx s->eip %lx\n", u->pc, s->eip);
#endif /* TARGET_X86_64 */
    u->flags = s->eflags;

}

// typedef struct CPUX86State {
//     /* standard registers */
//     target_ulong regs[CPU_NB_REGS];
//     target_ulong eip;
//     target_ulong eflags; /* eflags register. During CPU emulation, CC
//                         flags and DF are set to zero because they are
//                         stored elsewhere */

//     /* emulator internal eflags handling */
//     target_ulong cc_src;
//     target_ulong cc_dst;
//     uint32_t cc_op;
//     int32_t df; /* D flag : 1 if D = 0, -1 if D = 1 */
//     uint32_t hflags; /* TB flags, see HF_xxx constants. These flags
//                         are known at translation time. */
//     uint32_t hflags2; /* various other flags, see HF2_xxx constants. */

//     /* segments */
//     SegmentCache segs[6]; /* selector values */
//     SegmentCache ldt;
//     SegmentCache tr;
//     SegmentCache gdt; /* only base and limit are used */
//     SegmentCache idt; /* only base and limit are used */

//     target_ulong cr[5]; /* NOTE: cr1 is unused */
//     int32_t a20_mask;

//     /* FPU state */
//     unsigned int fpstt; /* top of stack index */
//     uint16_t fpus;
//     uint16_t fpuc;
//     uint8_t fptags[8];   /* 0 = valid, 1 = empty */
//     FPReg fpregs[8];
//     /* KVM-only so far */
//     uint16_t fpop;
//     uint64_t fpip;
//     uint64_t fpdp;

//     /* emulator internal variables */
//     float_status fp_status;
//     floatx80 ft0;

//     float_status mmx_status; /* for 3DNow! float ops */
//     float_status sse_status;
//     uint32_t mxcsr;
//     XMMReg xmm_regs[CPU_NB_REGS];
//     XMMReg xmm_t0;
//     MMXReg mmx_t0;
//     target_ulong cc_tmp; /* temporary for rcr/rcl */

//     /* sysenter registers */
//     uint32_t sysenter_cs;
//     target_ulong sysenter_esp;
//     target_ulong sysenter_eip;
//     uint64_t efer;
//     uint64_t star;

//     uint64_t vm_hsave;
//     uint64_t vm_vmcb;
//     uint64_t tsc_offset;
//     uint64_t intercept;
//     uint16_t intercept_cr_read;
//     uint16_t intercept_cr_write;
//     uint16_t intercept_dr_read;
//     uint16_t intercept_dr_write;
//     uint32_t intercept_exceptions;
//     uint8_t v_tpr;

// #ifdef TARGET_X86_64
//     target_ulong lstar;
//     target_ulong cstar;
//     target_ulong fmask;
//     target_ulong kernelgsbase;
// #endif
//     uint64_t system_time_msr;
//     uint64_t wall_clock_msr;
//     uint64_t async_pf_en_msr;
//     uint64_t pv_eoi_en_msr;

//     uint64_t tsc;
//     uint64_t tsc_deadline;

//     uint64_t mcg_status;
//     uint64_t msr_ia32_misc_enable;

//     /* exception/interrupt handling */
//     int error_code;
//     int exception_is_int;
//     target_ulong exception_next_eip;
//     target_ulong dr[8]; /* debug registers */
//     union {
//         CPUBreakpoint *cpu_breakpoint[4];
//         CPUWatchpoint *cpu_watchpoint[4];
//     }; /* break/watchpoints for dr[0..3] */
//     uint32_t smbase;
//     int old_exception;  /* exception in flight */

//     /* KVM states, automatically cleared on reset */
//     uint8_t nmi_injected;
//     uint8_t nmi_pending;

//     CPU_COMMON

//     uint64_t pat;

//     /* processor features (e.g. for CPUID insn) */
//     uint32_t cpuid_level;
//     uint32_t cpuid_vendor1;
//     uint32_t cpuid_vendor2;
//     uint32_t cpuid_vendor3;
//     uint32_t cpuid_version;
//     uint32_t cpuid_features;
//     uint32_t cpuid_ext_features;
//     uint32_t cpuid_xlevel;
//     uint32_t cpuid_model[12];
//     uint32_t cpuid_ext2_features;
//     uint32_t cpuid_ext3_features;
//     uint32_t cpuid_apic_id;
//     int cpuid_vendor_override;
//     /* Store the results of Centaur's CPUID instructions */
//     uint32_t cpuid_xlevel2;
//     uint32_t cpuid_ext4_features;
//     /* Flags from CPUID[EAX=7,ECX=0].EBX */
//     uint32_t cpuid_7_0_ebx_features;

//     /* MTRRs */
//     uint64_t mtrr_fixed[11];
//     uint64_t mtrr_deftype;
//     MTRRVar mtrr_var[8];

//     /* For KVM */
//     uint32_t mp_state;
//     int32_t exception_injected;
//     int32_t interrupt_injected;
//     uint8_t soft_interrupt;
//     uint8_t has_error_code;
//     uint32_t sipi_vector;
//     uint32_t cpuid_kvm_features;
//     uint32_t cpuid_svm_features;
//     bool tsc_valid;
//     int tsc_khz;
//     void *kvm_xsave_buf;

//     /* in order to simplify APIC support, we leave this pointer to the
//        user */
//     struct DeviceState *apic_state;

//     uint64_t mcg_cap;
//     uint64_t mcg_ctl;
//     uint64_t mce_banks[MCE_BANKS_DEF*4];

//     uint64_t tsc_aux;

//     /* vmstate */
//     uint16_t fpus_vmstate;
//     uint16_t fptag_vmstate;
//     uint16_t fpregs_format_vmstate;

//     uint64_t xstate_bv;
//     XMMReg ymmh_regs[CPU_NB_REGS];

//     uint64_t xcr0;

//     TPRAccess tpr_access_type;
// } CPUX86State;
#ifdef TARGET_X86_64

#endif /* TARGET_X86_64 */
#endif /* TARGET_I386 */


static void acid_read_byte(AcidState *s, int ch)
{
    static int i = 0;
    uint8_t reply;
    unsigned char body[10];
    static unsigned char mesg[10];
    static int new_session = 1;

    // XXX: this begs quite a few questions.
    target_ulong addr;
    target_ulong len;

    mesg[i] = ch;
    if(i < 9){
        i++;
        return;
    }
    i = 0;
    switch(mesg[0]) {
        case Tproc:
        // for initial proc message, send the number 1 back.
            reply = Rproc;
            body[0] = Rproc;
            snprintf(body+1, sizeof(body)-1, "%8.8lux", 1);
            put_buffer(s, body, 10);
            // if(new_session){
            //     body[0] = Rerr;
            //     put_buffer(s, body, 10);
            //     new_session = 0;     
            // }

            break;
        case Tmget:
            // read the rest of the message.
            memset(body, 0, 10);
            addr = ((mesg[1]<<24)|(mesg[2]<<16)|(mesg[3]<<8)|(mesg[4]<<0));
            len = mesg[5];
            if(len > 9) {
                // meaningful error message.
                printf("too long\n");
                body[0] = Rerr;
                body[1] = 0;
                put_buffer(s, body, 10);
                break;                
            }
            printf("addr 0x%lx len 0x%lx\n", addr, len);
            printf("Ureg %lx\n", sizeof(Ureg));


            if (addr < sizeof(Ureg)) {
                printf("reading ureg\n");
                // need some function to fill up packet from ureg.
                // which requires filling ureg when you need it.
                // which is really just a mapping of each to each
                // define a mapping function.
                //  need 
                cpu_to_ureg(s->g_cpu, ureg);
                body[0] = Rmget;
                printf("ureg %lx ureg+addr %lx\n", ureg, ureg+addr);
                printf("ureg %lx (uint64_t)ureg+addr %lx\n", ureg, ((uint64_t)ureg)+addr);
                // XXX: completely nonportable, need some way of knowing the hosts pointer size.
                body[1] = *((int*)((uint64_t)ureg+addr))&0xff;
                body[2] = (*((int*)((uint64_t)ureg+addr))>>8)&0xff;
                body[3] = (*((int*)((uint64_t)ureg+addr))>>16)&0xff;
                body[4] = (*((int*)((uint64_t)ureg+addr))>>24)&0xff;
                body[5] = 0;
// #ifdef TARGET_X86_64
//                 body[1] = ureg->ip&0xff;
//                 body[2] = (ureg->ip>>8)&0xff;
//                 body[3] = (ureg->ip>>16)&0xff;
//                 body[4] = (ureg->ip>>24)&0xff;
//                 body[5] = 0;
// #else
//                 body[1] = ureg->pc&0xff;
//                 body[2] = (ureg->pc>>8)&0xff;
//                 body[3] = (ureg->pc>>16)&0xff;
//                 body[4] = (ureg->pc>>24)&0xff;
//                 body[5] = 0;
// #endif /* TARGET_X86_64 */


                put_buffer(s, body, 10);              
            } else if (target_memory_rw_debug(s->g_cpu, addr, body+1, 4, 0) != 0) {
                // meaningful error message.
                printf("couldn't read\n");
                body[0] = Rerr;
                body[1] = 0;
                put_buffer(s, body, 10);
            } else {
                printf("read %lx from %lx\n", (body[1]<<24)|(body[2]<<16)|(body[3]<<8)|(body[4]<<0), addr);
                body[0] = Rmget;
                body[5] = 0;
                put_buffer(s, body, 10);
            }

            break;
        case Tmput:
            // read the rest of the message.
            memset(body, 0, 10);
            addr = ((mesg[1]<<24)|(mesg[2]<<16)|(mesg[3]<<8)|(mesg[4]<<0));
            printf("addr 0x%lx\n", addr);

            // Do I need to care about endianess here?
            printf("Ureg %lx", sizeof(Ureg));
            if (addr < sizeof(Ureg)) {
                printf("reading ureg\n");
                // are you allowed to write ureg in plan9?
                body[0] = Rmput;
                body[1] = 0;
                put_buffer(s, body, 10);
            } else if (target_memory_rw_debug(s->g_cpu, addr, mesg+6, mesg[5], 1) != 0) {
                printf("couldn't write\n");
                // meaningful error message.
                body[0] = Rerr;
                body[1] = 0;
                put_buffer(s, body, 10);
            } else {
                printf("wrote %lx to %lx\n", (mesg[6]<<24)|(mesg[7]<<16)|(mesg[8]<<8)|(mesg[9]<<0), addr);
                body[0] = Rmput;
                body[1] = 0;
                put_buffer(s, body, 10);
            }
            break;
        case Tstatus:
            memset(body, 0, 10);
            body[0] = Rstatus;
            // you are making the system emulate a process, therefore there needs to be a table that maps cpu state.
            strncpy(body+1, "Ready", sizeof(body)-1);
            put_buffer(s, body, 10);
            break;
        case Trnote:
            body[0] = Rrnote;
            body[1] = 0;
            put_buffer(s, body, 10);
            break;
        case Tstartstop:
            cpu_single_step(s->c_cpu, 1);
            body[0] = Rstartstop;
            body[1] = 0;
            put_buffer(s, body, 10);          
            break;
        case Twaitstop:
            put_buffer(s, "Twaitstop", 10);
            break;
        case Tstart:
            put_buffer(s, "Tstart", 10);
            break;
        case Tstop:
            vm_stop(RUN_STATE_PAUSED);
            body[0] = Rstop;
            body[1] = 0;
            put_buffer(s, body, 10);
            break;
        case Tkill:
        // unimplemented, XXX better error message.
            body[0] = Rkill;
            body[1] = 0;
            put_buffer(s, body, 10);            
            break;
        case Tcondbreak:
            body[0] = Rcondbreak;
            put_buffer(s, body, 10);
            break;
        default:
            printf("unknown char: %c\n", ch);
            break;
    }
	printf("%c", ch);
//     {
//         switch(s->state) {
//         case RS_IDLE:
//             if (ch == '$') {
//                 s->line_buf_index = 0;
//                 s->state = RS_GETLINE;
//             }
//             break;
//         case RS_GETLINE:
//             if (ch == '#') {
//             s->state = RS_CHKSUM1;
//             } else if (s->line_buf_index >= sizeof(s->line_buf) - 1) {
//                 s->state = RS_IDLE;
//             } else {
//             s->line_buf[s->line_buf_index++] = ch;
//             }
//             break;
//         case RS_CHKSUM1:
//             s->line_buf[s->line_buf_index] = '\0';
//             s->line_csum = fromhex(ch) << 4;
//             s->state = RS_CHKSUM2;
//             break;
//         case RS_CHKSUM2:
//             s->line_csum |= fromhex(ch);
//             csum = 0;
//             for(i = 0; i < s->line_buf_index; i++) {
//                 csum += s->line_buf[i];
//             }
//             if (s->line_csum != (csum & 0xff)) {
//                 reply = '-';
//                 put_buffer(s, &reply, 1);
//                 s->state = RS_IDLE;
//             } else {
//                 reply = '+';
//                 put_buffer(s, &reply, 1);
//                 s->state = gdb_handle_packet(s, s->line_buf);
//             }
//             break;
//         default:
//             abort();
//         }
//     }
}

#define MAX_PACKET_LENGTH 10

static int acid_chr_can_receive(void *opaque)
{
  /* We can handle an arbitrarily large amount of data.
   Pick the maximum packet size, which is as good as anything.  */
  return MAX_PACKET_LENGTH;
}

static void acid_chr_receive(void *opaque, const uint8_t *buf, int size)
{
    int i;

    for (i = 0; i < size; i++) {
        acid_read_byte(acidserver_state, buf[i]);
    }
}

static void acid_chr_event(void *opaque, int event)
{
    switch (event) {
    case CHR_EVENT_OPENED:
        vm_stop(RUN_STATE_PAUSED);
        // gdb_has_xml = 0;
        break;
    default:
        break;
    }
}


int acidserver_start(const char *device)
{
	// I need to be an echo server.
    AcidState *s;
	char acidstub_device_name[128];
    CharDriverState *chr = NULL;

	if (!device)
			return -1;
	if (strcmp(device, "none") != 0) {
		if (strstart(device, "tcp:", NULL)) {
            /* enforce required TCP attributes */
            snprintf(acidstub_device_name, sizeof(acidstub_device_name),
                     "%s,nowait,nodelay,server", device);
            device = acidstub_device_name;
        }
#ifndef _WIN32
        if (strcmp(device, "stdio") == 0) {
        	// XXX: need sigterm handler
            // struct sigaction act;

            // memset(&act, 0, sizeof(act));
            // act.sa_handler = acid_sigterm_handler;
            // sigaction(SIGINT, &act, NULL);
        }
#endif
		chr = qemu_chr_new("acid", device, NULL);
        if (!chr)
            return -1;

        qemu_chr_add_handlers(chr, acid_chr_can_receive, acid_chr_receive,
                              acid_chr_event, NULL);
	}
    s = acidserver_state;
    if (!s) {
        s = g_malloc0(sizeof(AcidState));
        acidserver_state = s;
        ureg = g_malloc0(sizeof(Ureg));
        // XXX I need to be able to handle state changes.
        // qemu_add_vm_change_state_handler(acid_vm_state_change, NULL);

        /* Initialize a monitor terminal for gdb */
        // mon_chr = g_malloc0(sizeof(*mon_chr));
        // mon_chr->chr_write = acid_monitor_write;
        // monitor_init(mon_chr, 0);
    } else {
        if (s->chr)
            qemu_chr_delete(s->chr);
        // mon_chr = s->mon_chr;
        memset(s, 0, sizeof(AcidState));
    }
    s->c_cpu = first_cpu;
    s->g_cpu = first_cpu;
    s->chr = chr;
    // XXX need some kind of state handling
    // s->state = chr ? RS_IDLE : RS_INACTIVE;
    // s->mon_chr = mon_chr;
    // s->current_syscall_cb = NULL;

    return 0;
    return 0;
}