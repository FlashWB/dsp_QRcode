/********************************************************************/
/*  Copyright 2004 by SEED Incorporated.							*/
/*  All rights reserved. Property of SEED Incorporated.				*/
/*  Restricted rights to use, duplicate or disclose this code are	*/
/*  granted through contract.									    */
/*  															    */
/********************************************************************/
/*���µĳ�������DMA��ʽ�ɼ�ͼ�ĵ����á��жϡ��Լ����ݴ���ĺ���*/
/**********************************************************/
/* Capture parameter definitions based on 525/60 format */
/**********************************************************/
/*NTSC*/
//#define VCA_HBLNK_SIZE 138 /* (858-720),horizontal blanking */
//#define VCA_IMG_VSIZE1 244 /* (263-20+1), fld1 vertical image size */
//#define VCA_IMG_VSIZE2 243 /* (525-283+1), fld2 vertical image size */

/*PAL*/
#define VCA_HBLNK_SIZE 144 /* (864-720),horizontal blanking */
/*ȷ����һ��ͼ�����ֱ����Ϊ288*/
#define VCA_IMG_VSIZE1 288 /* (311-24+1), fld1 vertical image size */
/*ȷ����һ��ͼ�����ֱ����Ϊ288*/
#define VCA_IMG_VSIZE2 288 /* (625-338+1), fld2 vertical image size */

/*ȷ����һ��ͼ���ˮƽ����Ϊ720*/
#define VCA_IMG_HSIZE1 720 /* field1 horizontal image size */
/*ȷ���ڶ���ͼ���ˮƽ����Ϊ720*/
#define VCA_IMG_HSIZE2 720 /* field2 horizontal image size */

/* ȷ��һ�����صĴ�СΪ720��288 */
#define VCA_IMAGE_SIZE1 (VCA_IMG_HSIZE1 * VCA_IMG_VSIZE1)
/* ȷ���������صĴ�СΪ720��288 */
#define VCA_IMAGE_SIZE2 (VCA_IMG_HSIZE2 * VCA_IMG_VSIZE2)
/* Define threshold values in double.words. Both fields should have same threshold value*/
/* ȷ��FIFO���¼����ż���Ϊһ�еĳ��ȣ���ΪDM642Ϊ64λDMA�����Գ���/8 */
#define VCA_VDTHRLD1 (VCA_IMG_HSIZE1/8) /* line length in */
#define VCA_VDTHRLD2 VCA_VDTHRLD1 /* double.words */
/* ....................................................... */
/* Define channel A capture window co-ordinates for Field1 */
/* ....................................................... */
/*����ͼ��Ĳɼ���*/
/* HRST = 0, start of horizontal blanking��HCOUNT��EAV��λ
   ��һ���Ǵ�EAV֮��ʼ ������������ǰ��ͼ���ں�*/
#define VCA_XSTART1 (VCA_HBLNK_SIZE-2)/*EAV*/
#define VCA_XSTOP1 (VCA_XSTART1 + VCA_IMG_HSIZE1-1)
/* VRST = 1, end of vertical blanking */
#define VCA_YSTART1 1
#define VCA_YSTOP1 (VCA_YSTART1 + VCA_IMG_VSIZE1-1)

/* ....................................................... */
/* Define channel A capture window co-ordinates for Field2 */
/* ....................................................... */
/* HRST = 0, start of horizontal blanking */
#define VCA_XSTART2 (VCA_HBLNK_SIZE-2/*EAV*/)
#define VCA_XSTOP2 (VCA_XSTART2 + VCA_IMG_HSIZE2-1)
/* VRST = 1, end of vertical blanking */
#define VCA_YSTART2 1
#define VCA_YSTOP2 (VCA_YSTART2 + VCA_IMG_VSIZE2-1)

/* Define threshold values in double-words. Both fields should same threshold value */
/* ȷ��FIFO�����ĳ���*/
#define VCA_THRLD_FIELD1 (VCA_IMG_HSIZE1/8) /* line length in */
#define VCA_THRLD_FIELD2 VCA_THRLD_FIELD1 /* double-words */

/* Define number of events to be generated for field1 and field2 
   �����泡��244����ż����������243��*/
#define VCA_CAPEVT1 (VCA_IMAGE_SIZE1 / (VCA_VDTHRLD1 * 8))
#define VCA_CAPEVT2 (VCA_IMAGE_SIZE2 / (VCA_VDTHRLD2 * 8))
/* in this example �����1֡  */
#define CAPCHA_FRAME_COUNT 1 
/* ............................................ */
/* EDMA parameters for capture Y event that are */
/* specific to this example. */
/* ............................................ */

/* because VCA_THRLD_FIELDn is in double-words and element size is 32-bit */
#define VCA_Y_EDMA_ELECNT (VCA_THRLD_FIELD1 * 2) 

#define VCA_Y_EDMA_FRMCNT ((VCA_CAPEVT1 + VCA_CAPEVT2) * CAPCHA_FRAME_COUNT)
/******************************************************************/
/* Description : 8.bit BT.656 non.continuous frame capture 		  */
/* 																  */
/* Some important field descriptions:                             */
/*                                                                */
/* CMODE = 000, 8.bit BT.656 mode                                 */
/* CON = 0                                                        */
/* FRAME = 1, capture frame                                       */
/* CF2 = 0                                                        */
/* CF1 = 0, (8-bit non.continuous frame capture)                  */
/* SCALE = 0, no scaling                                          */
/* RESMPL= 0, no resampling                                       */
/* 10BPK = X, not used in 8-bit capture                           */
/* EXC = 0, use EAV/SAV codes                                     */
/* VRST = 1, end of vertical blanking                             */
/* HRST = 0, start of horizontal blanking                         */
/* FLDD = 0, 1st line EAV or FID input                            */
/* FINV = 0, no field invert                                      */
/* RDFE = X, used in Raw mode only(Enable field identification)   */
/* SSE = X, used in Raw mode only(Startup synch enable)           */
/******************************************************************/
#include <vportcap.h>
/*................................................................ */
/* global variable declarations 								   */
/* ............................................................... */

	#pragma DATA_SECTION(capChaAYSpace, ".capChaAYSpace") 
	/* buffer to store captured Y-data */
	Uint8 capChaAYSpace[720*588]; 
	#pragma DATA_SECTION(capChaACbSpace, ".capChaACbSpace")
	/* buffer to store captured Cb-data */
	Uint8 capChaACbSpace[360*588]; 
	#pragma DATA_SECTION(capChaACrSpace, ".capChaACrSpace")
	/* buffer to store captured Cr-data */
	Uint8 capChaACrSpace[360*588];
	
	/* handle of vp that to be configured */
	VP_Handle vpCaptureHandle; 
	/*����Y��Cb��Cr��EDMAͨ·�ľ��*/ 
	EDMA_Handle hEdmaVPCapChaAY;
	EDMA_Handle hEdmaVPCapChaACb;
	EDMA_Handle hEdmaVPCapChaACr;
	
	/* EDMA tcc for Y channel */
	Int32 edmaCapChaAYTccNum = 0; 
	/* EDMA tcc for Cb channel */
	Int32 edmaCapChaACbTccNum = 0;
	/* EDMA tcc for Cb channel */ 
	Int32 edmaCapChaACrTccNum = 0;
	
	/* no of frames captured */ 
	volatile Uint32 capChaAFrameCount = 0; 
	
	/* Error flags */
	volatile Uint32 capChaAOverrun = 0;
	volatile Uint32 capChaASyncError = 0;
	volatile Uint32 capChaAShortFieldDetect = 0;
	volatile Uint32 capChaALongFieldDetect = 0;
	volatile Uint32 capNewFrame = 0;
/*******************************************************************/
/* Function : bt656_8bit_ncfc 									   */
/* Input(s) : portNumber, video port number i.e. 0, 1 or 2.        */
/* Description : Configures given video port for 8.bit BT.656 non. */
/* continuos frame capture on channel A.                           */
/*******************************************************************/
VP_Handle bt656_8bit_ncfc( int portNumber)
{
	/* Open video port for capture ,��һ����Ƶ�˿�*/
	vpCaptureHandle = VP_open(portNumber, VP_OPEN_RESET);
	if(vpCaptureHandle == INV)
	{
		return (VP_Handle)0xffff;
	}
	/* Enable video port functionality in VP Peripheral Control Reg(PCR)��ʹ����Ƶ�˿�*/
	VP_FSETH(vpCaptureHandle, PCR, PEREN, VP_PCR_PEREN_ENABLE);
	/* ..................... */
	/* Enable all interrupts */
	/* ..................... */
	/*����VCA���ж�Դ*/
	/* Enable capture overrun interrupt(COVRA) for VP channel A */
	VP_FSETH(vpCaptureHandle, VPIE, COVRA, VP_VPIE_COVRA_ENABLE);
	/* Enable capture complete interrupt(CCMPA) for VP channel A */
	VP_FSETH(vpCaptureHandle, VPIE, CCMPA, VP_VPIE_CCMPA_ENABLE);
	/* Enable channel synchronization error interrupt(SERRA) for */
	/* VP channel A */
	VP_FSETH(vpCaptureHandle, VPIE, SERRA, VP_VPIE_SERRA_ENABLE);
	/* Enable short field detect interrupt(SFDA) for VP channel A */
	VP_FSETH(vpCaptureHandle, VPIE, SFDA, VP_VPIE_SFDA_ENABLE);
	/* Enable video port global interrupt enable */
	VP_FSETH(vpCaptureHandle, VPIE, VIE, VP_VPIE_VIE_ENABLE);
	/* ...................... */
	/* Setup all other fields */
	/* ...................... */
	
	/* Enable short field detect��ʹ��ȱ��̽��*/
	VP_FSETH(vpCaptureHandle, VCACTL, SFDE, VP_VCACTL_SFDE_ENABLE);
	/* Set last pixel to be captured in Field1 (VCA_STOP1 reg) */
	/*���õ�һ�������һ�����ص�Y����X�������*/
	VP_RSETH(vpCaptureHandle, VCASTOP1,VP_VCASTOP1_RMK(VCA_YSTOP1, VCA_XSTOP1));
	/* Set last pixel to be captured in Field2 (VCA_STOP2 reg) */
	/*���õڶ��������һ�����ص�Y����X�������*/
	VP_RSETH(vpCaptureHandle, VCASTOP2,VP_VCASTOP2_RMK(VCA_YSTOP2, VCA_XSTOP2));
	/* Set first pixel to be captured in Field1 (VCA_STRT1 reg) */
	/*���õ�һ���ĵ�һ�����ص�Y����X�������*/
	VP_RSETH(vpCaptureHandle, VCASTRT1, VP_VCASTRT1_RMK(VCA_YSTART1,VP_VCASTRT1_SSE_ENABLE, VCA_XSTART1));
	/* Set first pixel to be captured in Field2 (VCA_STRT2 reg) */
	/*���õڶ����ĵ�һ�����ص�Y����X�������*/
	VP_RSETH(vpCaptureHandle, VCASTRT2,VP_VCASTRT2_RMK(VCA_YSTART2, VCA_XSTART2));
	/* Set threshold values ������EDMA����������*/
	VP_RSETH(vpCaptureHandle, VCATHRLD,VP_VCATHRLD_RMK(VCA_THRLD_FIELD2, VCA_THRLD_FIELD1));
	/* Set capture event.register values������һ������Ҫ��EDMA�Ĵ��� */
	VP_RSETH(vpCaptureHandle, VCAEVTCT,VP_VCAEVTCT_RMK(VCA_CAPEVT2,VCA_CAPEVT1));
	/* Vertical interrupts (VCA_INT) are not enabled in this example. */
	/* Set CMODE to 8.bit BT.656������BT656��ʽ�������� */
	VP_FSETH(vpCaptureHandle, VCACTL, CMODE, VP_VCACTL_CMODE_BT656B);
	/* Set non.continuous frame capture������Ϊ������ʾ��ʽ�Ĳɼ������������Ĳɼ���
	   CON/FRAME/CF2/CF1��ֵΪ0100 */
	VP_FSETH(vpCaptureHandle, VCACTL, CON, VP_VCACTL_CON_DISABLE);
	VP_FSETH(vpCaptureHandle, VCACTL, FRAME, VP_VCACTL_FRAME_FRMCAP);
	VP_FSETH(vpCaptureHandle, VCACTL, CF2, VP_VCACTL_CF2_NONE);
	VP_FSETH(vpCaptureHandle, VCACTL, CF1, VP_VCACTL_CF1_NONE);
	/* Let FLDD and FINV to be their defaults */
	/* Set VRST to end of vertical blanking,VCOUNT��λ�ڳ�����֮�� */
	VP_FSETH(vpCaptureHandle, VCACTL, VRST, VP_VCACTL_VRST_V0EAV);
	/* Set HRST to start of horizontal blanking���м�����λ��EAV֮�� */
	VP_FSETH(vpCaptureHandle, VCACTL, HRST, VP_VCACTL_HRST_OF(0));
	/* 10.bit pack mode(10BPK bit) in this 8.bit example */
	/* No (1/2) scaling and no chroma re.sampling in this example */
	/*��ʼ��EDMAͨ·*/
	IRQ_enable(IRQ_EVT_VINT1);
	/* Enable video port interrupts */
	IRQ_enable(vpCaptureHandle->eventId);
	/* Setup Y, Cb and Cr EDMA channels */
	setupVPCapChaAEDMA(portNumber);
	/* Clear VPHLT in VP_CTL to make video port function�����VPHLTλ?
	   ʹ������λ */
	VP_FSETH(vpCaptureHandle, VPCTL, VPHLT, VP_VPCTL_VPHLT_CLEAR);
	/* .............. */
	/* enable capture */
	/* .............. */
	/* set VCEN bit to enable capture��ʹ��VCA�� */
	VP_FSETH(vpCaptureHandle, VCACTL, VCEN, VP_VCACTL_VCEN_ENABLE);
	/* clear BLKCAP in VCA_CTL to enable capture DMA events */
	VP_FSETH(vpCaptureHandle, VCACTL, BLKCAP,VP_VCACTL_BLKCAP_CLEAR);
	
	return (vpCaptureHandle);
}
/*******************************************************************/
/* Function : bt656_capture_start    							   */
/* Input(s) : VP_handle									           */
/* Description : Configures given video port for 8.bit BT.656 non. */
/* continuos frame capture on channel A.                           */
/*******************************************************************/
void bt656_capture_start(VP_Handle videoHandle)
{	
}
/*................................................................ */
/* Function : VPCapChaAIsr */
/* Description : This capture ISR clears FRMC to continue capture */
/* in this non.continuous mode and also clears other */
/* status bits. */
/*................................................................ */
interrupt void VPCapChaAIsr(void)
{
	Uint32 vpis = 0;
	/* Get video port status register value */
	vpis = VP_RGETH(vpCaptureHandle, VPIS);
	if(vpis & _VP_VPIS_CCMPA_MASK) /* capture complete */
	{
		/* Clear frame complete bit in VCX_CTL to continue capture in non-continuous mode*/
		VP_FSETH(vpCaptureHandle, VCASTAT, FRMC,VP_VCASTAT_FRMC_CLEAR);
		/* Clear CCMPA to enable next frame complete interrupts*/
		VP_FSETH(vpCaptureHandle, VPIS, CCMPA,VP_VPIS_CCMPA_CLEAR);
		capChaAFrameCount++; /* increment captured frame count */
		capNewFrame = 1;
	}
	if(vpis & _VP_VPIS_COVRA_MASK) /* overrun error */
	{
		capChaAOverrun++;
		VP_FSETH(vpCaptureHandle, VPIS, COVRA,VP_VPIS_COVRA_CLEAR);
	}
	if(vpis & _VP_VPIS_SERRA_MASK) /* synchronization error */
	{
		capChaASyncError++;
		VP_FSETH(vpCaptureHandle, VPIS, SERRA,VP_VPIS_SERRA_CLEAR);
	}
	if(vpis & _VP_VPIS_SFDA_MASK) /* short field detect */
	{
		capChaAShortFieldDetect++;
		VP_FSETH(vpCaptureHandle, VPIS, SFDA, VP_VPIS_SFDA_CLEAR);
	}
	if(vpis & _VP_VPIS_LFDA_MASK) /* long field detect */
	{
		capChaALongFieldDetect++;
		VP_FSETH(vpCaptureHandle, VPIS, LFDA, VP_VPIS_LFDA_CLEAR);
	}
}
/*................................................................ */
/* Function : setupVPCapChaAEDMA(Int32 portNumber) */
/* Input(s) : portNumber, video port number i.e. 0, 1 or 2. */
/* Description : Sets up EDMA channels for Y, U, V events for */
/* channel A capture. */
/*������������ΪY��U��V����EDMA��ͨ·*/
/*................................................................ */
void setupVPCapChaAEDMA(int portNumber)
{
	Int32 YEvent, UEvent, VEvent;
	/* get channelA Y, U, V EDMA event numbers��ѡ�񴥷��¼� */
	switch(portNumber)
	{
		/*ȷ����Ӧ��DMAͨ·*/
		case VP_DEV0: 
			YEvent = EDMA_CHA_VP0EVTYA;
			UEvent = EDMA_CHA_VP0EVTUA;
			VEvent = EDMA_CHA_VP0EVTVA;
			break;
		case VP_DEV1: 
			YEvent = EDMA_CHA_VP1EVTYA;
			UEvent = EDMA_CHA_VP1EVTUA;
			VEvent = EDMA_CHA_VP1EVTVA;
			break;
		case VP_DEV2: 
			YEvent = EDMA_CHA_VP2EVTYA;
			UEvent = EDMA_CHA_VP2EVTUA;
			VEvent = EDMA_CHA_VP2EVTVA;
			break;
	}
	/* Configure Y EDMA channel to move data from YSRCA */
	/* (FIFO) to Y.data buffer, capChaAYSpace */
	configVPCapEDMAChannel( &hEdmaVPCapChaAY, YEvent,
							&edmaCapChaAYTccNum,
							vpCaptureHandle->ysrcaAddr,
							(Uint32)capChaAYSpace,
							VCA_Y_EDMA_FRMCNT,
							VCA_Y_EDMA_ELECNT);
	/* Configure Cb EDMA channel to move data from CbSRCA */
	/* (FIFO) to Cb.data buffer, capChaACbSpace */
	configVPCapEDMAChannel( &hEdmaVPCapChaACb, UEvent,
							&edmaCapChaACbTccNum,
							vpCaptureHandle->cbsrcaAddr,
							(Uint32)capChaACbSpace,
							VCA_Y_EDMA_FRMCNT,
							VCA_Y_EDMA_ELECNT/2); /* (1/2) of Y.samples */
	/* Configure Cr EDMA channel to move data from CrSRCA */
	/* (FIFO) to Cr.data buffer, capChaACrSpace */
	configVPCapEDMAChannel( &hEdmaVPCapChaACr, VEvent,
							&edmaCapChaACrTccNum,
							vpCaptureHandle->crsrcaAddr,
							(Uint32)capChaACrSpace,
							VCA_Y_EDMA_FRMCNT,
							VCA_Y_EDMA_ELECNT/2); /* (1/2) of Y.samples */
	/* Enable three EDMA channels */
	EDMA_enableChannel(hEdmaVPCapChaAY);
	EDMA_enableChannel(hEdmaVPCapChaACb);
	EDMA_enableChannel(hEdmaVPCapChaACr);
}
/*................................................................ */
/* Function : configVPCapEDMAChannel */
/* */
/* Input(s) : edmaHandle . pointer to EDMA handle. */
/* eventId . EDMA eventId. */
/* tccNum . pointer to transfer complete number. */
/* srcAddr . source address for EDMA transfer. */
/* dstAddr . destination address for EDMA transfer */
/* frameCount . frame count. */
/* elementCount . element count(32.bit element size). */
/* */
/* Output(s): edmaHandle . edma Handle of the given event. */
/* tccNum . transfer complete code for the given */
/* event. */
/* */
/* Description : Configures the given VP capture EDMA channel. */
/* The source address update is fixed address mode */
/* because the captured data is read from the FIFO. */
/* In this example, the destination address mode is */
/* auto.increment. But, in real.time applications */
/* there is lot of flexibility in the way capture */
/* buffers can be managed like ping.pong and round */
/* robin,��etc. */
/*................................................................ */
void configVPCapEDMAChannel(EDMA_Handle *edmaHandle, 
							Int32 eventId,
							Int32 *tccNum, 
							Uint32 srcAddr,
							Uint32 dstAddr, 
							Uint32 frameCount,
							Uint32 elementCount)
{
	Int32 tcc = 0;
	EDMA_Handle hEdmaTable;
	/* Open Y EVT EDMA channel */
	*edmaHandle = EDMA_open(eventId, EDMA_OPEN_RESET);
	if(*edmaHandle == EDMA_HINV)
	{
		for(;;){}
	}	
	/* allocate TCC for Y event */
	if((tcc = EDMA_intAlloc(-1)) == -1)
	{
		for(;;){}
	}
	/*��һ���µ�EDMA����*/
	hEdmaTable = EDMA_allocTable(-1);
	/* Configure EDMA parameters */
	EDMA_configArgs(
		*edmaHandle,
		EDMA_OPT_RMK(EDMA_OPT_PRI_MEDIUM, /* medium priority �������ȼ�Ϊ��*/
					 EDMA_OPT_ESIZE_32BIT, /* Element size 32 bits Ԫ�صĳ���Ϊ4��Byte*/
					 EDMA_OPT_2DS_NO, /* 1.dimensional source(FIFO) Դ���ù̶��ķ���*/
					 EDMA_OPT_SUM_NONE, /* fixed src address mode(FIFO) Դ��ַ����*/
					 EDMA_OPT_2DD_YES, /* 2.dimensional destination Ŀ�Ĳ���2ά�ռ�����*/
					 EDMA_OPT_DUM_INC, /* destination increment Ŀ�Ĳ��������ķ�ʽ*/
					 EDMA_OPT_TCINT_YES, /* Enable transfer complete indication��ʹ�ܴ��ͽ���ָʾ*/
					 EDMA_OPT_TCC_OF(tcc & 0xF),/*������ɱ�־CIP0�ĵ�λ*/
					 EDMA_OPT_TCCM_OF(((tcc & 0x30) >> 4)),/*������ɱ�־CIP0�ĸ�λ*/
					 EDMA_OPT_ATCINT_NO, /* Disable Alternate Transfers ��ֹ���洫�� */
					 EDMA_OPT_ATCC_OF(0),/* Complete Interrupt δʹ�ý������־ */
					 EDMA_OPT_PDTS_DISABLE, /* disable PDT(peripheral device transfer) mode for source */
					 EDMA_OPT_PDTD_DISABLE, /* disable PDT mode for dest */
					 EDMA_OPT_LINK_YES, /* Disable linking */
					 EDMA_OPT_FS_NO), /* Array synchronization ����ͬ��*/
		EDMA_SRC_RMK(srcAddr),/*Դ��ַ*/
		EDMA_CNT_RMK(EDMA_CNT_FRMCNT_OF((frameCount-1)),/*���е�����*/
					 EDMA_CNT_ELECNT_OF(elementCount)),/*ÿ�е�������*/
		EDMA_DST_RMK(dstAddr),/*Ŀ�ĵ�ַ*/
		EDMA_IDX_RMK(EDMA_IDX_FRMIDX_OF((elementCount * 4)),/*ÿ�е�����*/
		         	 EDMA_IDX_ELEIDX_OF(0)), /* note: 32-bit element size */
		/* no RLD in 2D and no linking */
		EDMA_RLD_RMK(EDMA_RLD_ELERLD_OF(0), EDMA_RLD_LINK_OF(0))
	);
	EDMA_configArgs(
		hEdmaTable,/*���ñ�ľ��*/
		EDMA_OPT_RMK(EDMA_OPT_PRI_MEDIUM, /* medium priority �������ȼ�Ϊ��*/
					 EDMA_OPT_ESIZE_32BIT, /* Element size 32 bits Ԫ�صĳ���Ϊ4��Byte*/
					 EDMA_OPT_2DS_NO, /* 1.dimensional source(FIFO) Դ���ù̶��ķ���*/
					 EDMA_OPT_SUM_NONE, /* fixed src address mode(FIFO) Դ��ַ����*/
					 EDMA_OPT_2DD_YES, /* 2.dimensional destination Ŀ�Ĳ���2ά�ռ�����*/
					 EDMA_OPT_DUM_INC, /* destination increment Ŀ�Ĳ��������ķ�ʽ*/
					 EDMA_OPT_TCINT_YES, /* Enable transfer complete indication��ʹ�ܴ��ͽ���ָʾ*/
					 EDMA_OPT_TCC_OF(tcc & 0xF),/*������ɱ�־CIP0�ĵ�λ*/
					 EDMA_OPT_TCCM_OF(((tcc & 0x30) >> 4)),/*������ɱ�־CIP0�ĸ�λ*/
					 EDMA_OPT_ATCINT_NO, /* Disable Alternate Transfers ��ֹ���洫�� */
					 EDMA_OPT_ATCC_OF(0),/* Complete Interrupt δʹ�ý������־ */
					 EDMA_OPT_PDTS_DISABLE, /* disable PDT(peripheral device transfer) mode for source */
					 EDMA_OPT_PDTD_DISABLE, /* disable PDT mode for dest */
					 EDMA_OPT_LINK_YES, /* Disable linking */
					 EDMA_OPT_FS_NO), /* Array synchronization ����ͬ��*/
		EDMA_SRC_RMK(srcAddr),/*Դ��ַ*/
		EDMA_CNT_RMK(EDMA_CNT_FRMCNT_OF((frameCount-1)),/*���е�����*/
					 EDMA_CNT_ELECNT_OF(elementCount)),/*ÿ�е�������*/
		EDMA_DST_RMK(dstAddr),/*Ŀ�ĵ�ַ*/
		EDMA_IDX_RMK(EDMA_IDX_FRMIDX_OF((elementCount * 4)),/*ÿ�е�����*/
		         	 EDMA_IDX_ELEIDX_OF(0)), /* note: 32.bit element size */
		/* no RLD in 2D and no linking */
		EDMA_RLD_RMK(EDMA_RLD_ELERLD_OF(0), EDMA_RLD_LINK_OF(0))
	);
	/*��������DMA*/
	EDMA_link(*edmaHandle,hEdmaTable);
	/*ʹEDMAѭ������*/
	EDMA_link(hEdmaTable,hEdmaTable);
	*tccNum = tcc;/*�����ж���ɱ�־*/
}
