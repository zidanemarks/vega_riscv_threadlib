    //uint32 sp;
    //union
    //{
    //   uint32 reg[15];
    //    struct
    //    {
    //        uint32 r15;
    //        uint32 r17;
    //        uint32 r19;
    //        uint32 r20;
    //        uint32 r21;
    //        uint32 r22;
    //        uint32 r23;
    //        uint32 r24;
    //        uint32 r25;
    //        uint32 r26;
    //        uint32 r27;
    //        uint32 r28;
    //        uint32 r29;
    //        uint32 r30;
    //        uint32 r31;
    //    };
    //};

	.section .text
	.globl	__Yield
	.align	4
    .ent	__Yield
        .type __Yield, @function
__Yield:
	//save registers
	swi	r15,r5,4
	swi	r17,r5,8
	swi	r19,r5,12
	swi	r20,r5,16
	swi	r21,r5,20
	swi	r22,r5,24
	swi	r23,r5,28
	swi	r24,r5,32
	swi	r25,r5,36
	swi	r26,r5,40
	swi	r27,r5,44
	swi	r28,r5,48
	swi	r29,r5,52
	swi	r30,r5,56
	swi	r31,r5,60
	//store R1 in *poldctx->sp
	swi	r1,r5,0
	//set R1 to *pnewctx->sp
	lwi	r1,r6,0
	//restore registers
	lwi	r15,r6,4
	lwi	r17,r6,8
	lwi	r19,r6,12
	lwi	r20,r6,16
	lwi	r21,r6,20
	lwi	r22,r6,24
	lwi	r23,r6,28
	lwi	r24,r6,32
	lwi	r25,r6,36
	lwi	r26,r6,40
	lwi	r27,r6,44
	lwi	r28,r6,48
	lwi	r29,r6,52
	lwi	r30,r6,56
	rtsd	r15,8
	lwi	r31,r6,60
	.end	__Yield
