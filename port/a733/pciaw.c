/*
 * Allwinner A733 PCIe host controller stub
 *
 * Based on arm64/pciqemu.c structure.
 * TODO: implement DesignWare PCIe initialization.
 */
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "../port/pci.h"

typedef struct Intvec Intvec;
struct Intvec {
	Pcidev *p;
	void (*f)(Ureg*, void*);
	void *a;
};

static Intvec vec[64];

static void
awpciintr(Ureg *u, void*)
{
	Intvec *v;
	for(v = vec; v < vec + nelem(vec); v++)
		if(v->f != nil)
			v->f(u, v->a);
}

void
pciintrenable(int tbdf, void (*f)(Ureg*, void*), void *a)
{
	Intvec *v;

	USED(tbdf);
	for(v = vec; v < vec + nelem(vec); v++){
		if(v->f == nil){
			v->f = f;
			v->a = a;
			return;
		}
	}
}

void
pciintrdisable(int tbdf, void (*f)(Ureg*, void*), void *a)
{
	Intvec *v;

	USED(tbdf);
	for(v = vec; v < vec + nelem(vec); v++){
		if(v->f == f && v->a == a){
			v->f = nil;
			v->a = nil;
			return;
		}
	}
}

int
pcicfgrw8(int tbdf, int rno, int data, int read)
{
	USED(tbdf); USED(rno); USED(data); USED(read);
	return 0;
}

int
pcicfgrw16(int tbdf, int rno, int data, int read)
{
	USED(tbdf); USED(rno); USED(data); USED(read);
	return 0;
}

int
pcicfgrw32(int tbdf, int rno, int data, int read)
{
	USED(tbdf); USED(rno); USED(data); USED(read);
	return 0;
}

void
pciawlink(void)
{
	/* TODO: initialize DesignWare PCIe controller */
	/* For now, no PCI devices will be discovered */
	print("pciaw: stub — PCIe not yet implemented\n");
}
