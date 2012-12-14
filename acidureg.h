typedef struct Ureg
{
	target_ulong 	di;		/* general registers */
	target_ulong 	si;		/* ... */
	target_ulong 	bp;		/* ... */
	target_ulong 	nsp;
	target_ulong 	bx;		/* ... */
	target_ulong 	dx;		/* ... */
	target_ulong 	cx;		/* ... */
	target_ulong 	ax;		/* ... */
	target_ulong 	gs;		/* data segments */
	target_ulong 	fs;		/* ... */
	target_ulong 	es;		/* ... */
	target_ulong 	ds;		/* ... */
	target_ulong 	trap;		/* trap type */
	target_ulong 	ecode;		/* error code (or zero) */
	target_ulong 	pc;		/* pc */
	target_ulong 	cs;		/* old context */
	target_ulong 	flags;		/* old flags */
	union {
		target_ulong 	usp;
		target_ulong 	sp;
	} u0;
	target_ulong 	ss;		/* old stack segment */
} Ureg;
typedef struct Ureg {
	target_ullong	ax;
	target_ullong	bx;
	target_ullong	cx;
	target_ullong	dx;
	target_ullong	si;
	target_ullong	di;
	target_ullong	bp;
	target_ullong	r8;
	target_ullong	r9;
	target_ullong	r10;
	target_ullong	r11;
	target_ullong	r12;
	target_ullong	r13;
	target_ullong	r14;
	target_ullong	r15;

	target_ushort	ds;
	target_ushort	es;
	target_ushort	fs;
	target_ushort	gs;

	target_ullong	type;
	target_ullong	error;		/* error code (or zero) */
	target_ullong	ip;		/* pc */
	target_ullong	cs;		/* old context */
	target_ullong	flags;		/* old flags */
	target_ullong	sp;		/* sp */
	target_ullong	ss;		/* old stack segment */
} Ureg;
