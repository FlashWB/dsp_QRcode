/********************************************************************/
/*  Copyright 2004 by SEED Incorporated.							*/
/*  All rights reserved. Property of SEED Incorporated.				*/
/*  Restricted rights to use, duplicate or disclose this code are	*/
/*  granted through contract.									    */
/*  															    */
/********************************************************************/

/********************************************************************/
/*                    ͼ���ƽ������˹ģ�壩                        */ 
/********************************************************************/


#include <csl.h>
#include <csl_emifa.h>
#include <csl_i2c.h>
#include <csl_gpio.h>
#include <csl_irq.h>
#include <csl_chip.h>
#include <csl_dat.h>
#include "iic.h"
#include "vportcap.h"
#include "vportdis.h"
#include "sa7121h.h"
#include "TVP51xx.h"
#include "seeddm642.h"
#include  <math.h>
/*SEEDDM642��emifa�����ýṹ*/
EMIFA_Config Seeddm642ConfigA ={
	   0x00052078,/*gblctl EMIFA(B)global control register value */
	   			  /*��CLK6��4��1ʹ�ܣ���MRMODE��1��ʹ��EK2EN,EK2RATE*/
	   0xffffffd3,/*cectl0 CE0 space control register value*/
	   			  /*��CE0�ռ���ΪSDRAM*/
	   0x73a28e01,/*cectl1 CE1 space control register value*/
	   			  /*Read hold: 1 clock;
	   			    MTYPE : 0000,ѡ��8λ���첽�ӿ�
	   			    Read strobe ��001110��14��clock���
	   			    TA��2 clock; Read setup 2 clock;
	   			    Write hold :2 clock; Write strobe: 14 clock
	   			    Write setup :7 clock
	   			    --					 ---------------
	   			  	  \		 14c		/1c
	   			 	   \----------------/ */
	   0x22a28a22, /*cectl2 CE2 space control register value*/
       0x22a28a42, /*cectl3 CE3 space control register value*/
	   0x57115000, /*sdctl SDRAM control register value*/
	   0x0000081b, /*sdtim SDRAM timing register value*/
	   0x001faf4d, /*sdext SDRAM extension register value*/
	   0x00000002, /*cesec0 CE0 space secondary control register value*/
	   0x00000002, /*cesec1 CE1 space secondary control register value*/
	   0x00000002, /*cesec2 CE2 space secondary control register value*/
	   0x00000073 /*cesec3 CE3 space secondary control register value*/	
};

/*SEEDDM642��IIC�����ýṹ*/
I2C_Config SEEDDM642IIC_Config = {
    0,  /* master mode,  i2coar;������ģʽ   */
    0,  /* no interrupt, i2cimr;ֻд���������������жϷ�ʽ*/
    (20-5), /* scl low time, i2cclkl;  */
    (20-5), /* scl high time,i2cclkh;  */
    1,  /* configure later, i2ccnt;*/
    0,  /* configure later, i2csar;*/
    0x4ea0, /* master tx mode,     */
            /* i2c runs free,      */
            /* 8-bit data + NACK   */
            /* no repeat mode      */
    (75-1), /* 4MHz clock, i2cpsc  */
};

CHIP_Config SEEDDM642percfg = {
	CHIP_VP2+\
	CHIP_VP1+\
	CHIP_VP0+\
	CHIP_I2C
};

I2C_Handle hSeeddm642i2c;
int portNumber;
extern SA7121H_ConfParams sa7121hPAL[45];
extern SA7121H_ConfParams sa7121hNTSC[45];
Uint8 vFromat = 0;
Uint8 misc_ctrl = 0x6D;
Uint8 output_format = 0x47;
// ��ַΪ0 for cvbs port1,ѡ�񸴺��ź���Ϊ����
Uint8 input_sel = 0x00;
/*��ַΪ0xf����Pin27���ó�ΪCAPEN����*/	
Uint8 pin_cfg = 0x02;
/*��ַΪ1B*/
Uint8 chro_ctrl_2 = 0x14;
/*ͼ����������*/
VP_Handle vpHchannel0;
VP_Handle vpHchannel1;
VP_Handle vpHchannel2;

/*ȷ��ͼ��Ĳ���*/
int numPixels = 720;//ÿ��720������
int numLines  = 576;//ÿ֡576�У�PAL��

/*ȷ������ķ�Χ*/
/*A             */
/*              */
/*             D*/ 
int intAPixels = 190;
int intALines = 59;
int intDPixels = 530;
int intDLines = 229; 

/*******************/
/*****�����α߿���������*****/
void drawRectangle();
/*ƽ��������˹ģ�壩*/
void GaussSmooth();

void threshold();
void Allpicture();
void Dispicture();
void sobelEdge();
void LaplacianEdge();
float getAngle();
void rotate();

/*�ɼ�����ʾ����������ַ*/
Uint32 capYbuffer  = 0x80000000;
Uint32 capCbbuffer = 0x800675c0;
Uint32 capCrbuffer = 0x8009b0a0;

Uint32 disYbuffer  = 0x80100000;
Uint32 disCbbuffer = 0x801675c0; 
Uint32 disCrbuffer = 0x8019b0a0;

Uint32 tempSrcYbuffer    = 0x80200000;   //��ʱ
Uint32 tempDisYbuffer    = 0x80300000;   //��ʱ
Uint32 tempYbuffer		 = 0x80400000;   //��ʱ���˹�����  //��ʱ���Ե��ȡ��
Uint32 tempAllbuffer	 = 0x80500000;
Uint32 tempDybuffer		 = 0x80600000;
Uint32 tempThrebuffer	 = 0x80700000;	 //��ʱ���ֵ��

/*ͼ���ʽ��־*/
Uint8 NTSCorPAL = 0;
extern far void vectors();
extern volatile Uint32 capNewFrame;
extern volatile Uint32 disNewFrame;

/*�˳���ɽ��ĸ��ɼ��ڵ����ݾ���Video Port0�ͳ�*/
void main()
{
	Uint8 addrI2C;
	int i;
	
/*-------------------------------------------------------*/
/* perform all initializations                           */
/*-------------------------------------------------------*/
	/*Initialise CSL����ʼ��CSL��*/
	CSL_init();
	CHIP_config(&SEEDDM642percfg);
/*----------------------------------------------------------*/
	/*EMIFA�ĳ�ʼ������CE0��ΪSDRAM�ռ䣬CE1��Ϊ�첽�ռ�
	 ע��DM642֧�ֵ���EMIFA������EMIF*/
	EMIFA_config(&Seeddm642ConfigA);
/*----------------------------------------------------------*/
	/*�ж�������ĳ�ʼ��*/
	//Point to the IRQ vector table
    IRQ_setVecs(vectors);
    IRQ_nmiEnable();
    IRQ_globalEnable();
    IRQ_map(IRQ_EVT_VINT1, 11);
    IRQ_map(IRQ_EVT_VINT0, 12);
    IRQ_reset(IRQ_EVT_VINT1);
    IRQ_reset(IRQ_EVT_VINT1);
    /*��һ�����ݿ���������ͨ·*/
    DAT_open(DAT_CHAANY, DAT_PRI_LOW, DAT_OPEN_2D);	
/*----------------------------------------------------------*/	
	/*����IIC�ĳ�ʼ��*/
	hSeeddm642i2c = I2C_open(I2C_PORT0,I2C_OPEN_RESET);
	I2C_config(hSeeddm642i2c,&SEEDDM642IIC_Config);
/*----------------------------------------------------------*/
	/*����TVP5150pbs�ĳ�ʼ��*/
	/*ѡ��TVP5150�����õ���ͨ·*/
	GPIO_RSET(GPGC,0x0);/*��GPIO0����ΪGPINTʹ��*/
	GPIO_RSET(GPDIR,0x1);/*��GPIO0��Ϊ���*/
	GPIO_RSET(GPVAL,0x0);/*GPIO0���Ϊ�ߣ�ѡ��IIC1���ߣ�����
						   �ڶ�·����ΪU21*/
	addrI2C = 0xBA >>1;
    _IIC_write(hSeeddm642i2c, addrI2C,0x00, input_sel);
    _IIC_write(hSeeddm642i2c, addrI2C,0x03, misc_ctrl);
    _IIC_write(hSeeddm642i2c, addrI2C,0x0D, output_format);
    _IIC_write(hSeeddm642i2c, addrI2C,0x0F, pin_cfg);
    _IIC_write(hSeeddm642i2c, addrI2C,0x1B, chro_ctrl_2);
    /*�ض���ǰ�����豸�ĸ�ʽ*/
    _IIC_read(hSeeddm642i2c, addrI2C,0x8c, &vFromat);
    vFromat = vFromat & 0xff;
	switch (vFromat)
	{
		case TVP51XX_NTSCM:
		case TVP51XX_NTSC443:
			NTSCorPAL = 1;/*ϵͳΪNTSC��ģʽ*/
			break;
		case TVP51XX_PALBGHIN:
		case TVP51XX_PALM:
			NTSCorPAL = 0;/*ϵͳΪPAL��ģʽ*/
			break;
		default:
			NTSCorPAL = 2;/*ϵͳΪ��֧�ֵ�ģʽ*/
			break;
	}
	if(NTSCorPAL ==2)
	{
		/*ϵͳ��֧�ֵ�ģʽ����������*/
		for(;;)
		{}
	}  		  
/*----------------------------------------------------------*/	
	/*����SAA7121H�ĳ�ʼ��*/
	GPIO_RSET(GPVAL,0x0);/*GPIO0���Ϊ�ͣ�ѡ��IIC1���ߣ�����ͼ�����*/						  
	addrI2C = 0xB8 >>1; /*ѡ���0·��I2C�ĵ�ַ*/
	/*����0·����Ƶ����ڵ����ݿ���Ϊ����״̬��
	  ʹ��SCLK������27����Ϊ����*/
	_IIC_write(hSeeddm642i2c, addrI2C,0x03, 0x1);
	/*����SAA7121H*/
	GPIO_RSET(GPVAL,0x1);/*GPIO0���Ϊ�ͣ�ѡ��IIC1���ߣ�����ͼ�����*/	
	/*��ʼ��Video Port0*/
	/*��Vedio Port1��Ϊencoder���*/
	portNumber = 0;
	vpHchannel0 = bt656_8bit_ncfd(portNumber);
	
    for(i = 0; i < 100000; i ++);
	SEEDDM642_rset(SEEDDM642_WDOGEN,2);
	
	addrI2C = 0x88 >>1;					      
	for(i =0; i<43; i++)
	{
		if(NTSCorPAL == 1)
		{
			_IIC_write(hSeeddm642i2c, 
					   addrI2C,
					   (sa7121hNTSC[i].regsubaddr), 
					   (sa7121hNTSC[i].regvule));
		}
		else
		{
			_IIC_write(hSeeddm642i2c, 
					   addrI2C,
					   (sa7121hPAL[i].regsubaddr), 
					   (sa7121hPAL[i].regvule));	
		}		
	}
	
/*----------------------------------------------------------*/
	/*��ʼ��Video Port1*/
	/*��Vedio Port1��Ϊ�ɼ�����*/
	portNumber = 1;
	vpHchannel1 = bt656_8bit_ncfc(portNumber);
	bt656_capture_start(vpHchannel1);
	/*�ȴ���һ֡���ݲɼ����*/
	while(capNewFrame == 0){}
	/*�����ݴ�����ʾ������������ɼ���ɵı�־*/
	capNewFrame =0;
	for(i=0;i<numLines;i++)
	{
		/*������ʱY������*/
		DAT_copy((void *)(capYbuffer + i * numPixels), 
	             (void *)(tempSrcYbuffer + i * numPixels),
	             numPixels);
		/*������ʱY������*/
		DAT_copy((void *)(capYbuffer + i * numPixels), 
	             (void *)(tempDisYbuffer + i * numPixels),
	             numPixels);	             	  	 
	 }

	//�ϲ�
	Allpicture(tempAllbuffer, tempSrcYbuffer);
	/*ƽ��������˹ģ�壩*/
    GaussSmooth(); 
    threshold();
	LaplacianEdge();
	//Allpicture(tempYbuffer,tempAllbuffer);
	rotate(getAngle());
	//getAngle();

    /*���߿�*/    
    drawRectangle();
	 
	for(i=0;i<numLines;i++)
	{
		/*����Y������*/
		DAT_copy((void *)(tempDisYbuffer + i * numPixels), 
	             (void *)(disYbuffer + i * numPixels),
	             numPixels);
	 }	 
	
	//�����ɫ�ź�  
	for(i=0;i<0x33ae0;i++)
	{
	    *((Uint8 *)(disCrbuffer +i)) =0x80;
	    *((Uint8 *)(disCbbuffer +i)) =0x80;

	} 
	 
	 
	/*������ʾģ��*/
	bt656_display_start(vpHchannel0);
	/*������ʾ��ʵʱѭ��*/
	for(;;)
	{
		/*���ɼ����������Ѿ��ɼ��ã�����ʾ�������������ѿ�*/
		if((capNewFrame == 1)&&(disNewFrame == 1))
		{
			/*������װ����ʾ������������ɼ���ɵı�־*/
			capNewFrame =0;
			disNewFrame =0;
			for(i=0;i<numLines;i++)
			{
				/*������ʱY������*/
				DAT_copy((void *)(capYbuffer + i * numPixels), 
			             (void *)(tempSrcYbuffer + i * numPixels),
			             numPixels);
 			  	/*������ʱY������*/
				DAT_copy((void *)(capYbuffer + i * numPixels), 
			             (void *)(tempDisYbuffer + i * numPixels),
			             numPixels);
			 }

		    //�ϲ�
			Allpicture(tempAllbuffer, tempSrcYbuffer);
			/*ƽ��������˹ģ�壩*/
            GaussSmooth(); 	
            threshold();
			LaplacianEdge();
			//Allpicture(tempYbuffer,tempAllbuffer);
			rotate(getAngle());
			//getAngle();

		    /*���߿�*/    
		    drawRectangle();
		    			 
			for(i=0;i<numLines;i++)
			{
				/*����Y������*/
				DAT_copy((void *)(tempDisYbuffer + i * numPixels), 
			             (void *)(disYbuffer + i * numPixels),
			             numPixels);		
			 }
		}
		
	}
	
	for(;;)
	{}
/*----------------------------------------------------------*/
	/*�ɼ���ط�*/	
}  

/*�����α߿����Ķ���*/
void drawRectangle()
{
    int i,j;
    /*���ϱ�*/
    //������
    for(i=intALines-4;i<intALines;i++)  //����
	{
	    for(j=intAPixels-6;j<intDPixels+6;j++) //���ص���
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}	
	
	//ż����
	for(i=numLines/2+intALines-4;i<numLines/2+intALines;i++)  //����
	{
	    for(j=intAPixels-6;j<intDPixels+6;j++) //���ص���
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}


	/*���±�*/
	//������
    for(i=intDLines;i<intDLines+4;i++)//����
	{
	    for(j=intAPixels-6;j<intDPixels+6;j++) //���ص���
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}
	
	//ż����
	for(i=numLines/2+intDLines;i<numLines/2+intDLines+4;i++)//����
	{
	    for(j=intAPixels-6;j<intDPixels+6;j++) //���ص���
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}	

	
	/*�����*/
	//������
    for(i=intALines;i<intDLines;i++)//����
	{
	    for(j=intAPixels-6;j<intAPixels;j++) //���ص���
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}
	
	//ż����
	for(i=numLines/2+intALines;i<numLines/2+intDLines;i++)//����
	{
	    for(j=intAPixels-6;j<intAPixels;j++) //���ص���
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}
	
	
	/*���ұ�*/
	//������
    for(i=intALines;i<intDLines;i++)//����
	{
	    for(j=intDPixels;j<intDPixels+6;j++) //���ص���
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}
	
	//ż����
    for(i=numLines/2+intALines;i<numLines/2+intDLines;i++)//����
	{
	    for(j=intDPixels;j<intDPixels+6;j++) //���ص���
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}
} 

void Allpicture(Uint32 tempall, Uint32 tempold)
{
	//int numPixels = 720;//ÿ��720������
	//int numLines  = 576;//ÿ֡576�У�PAL��
	int i,j;
	int ji=0, ou=0;
	int index;
	for (index = 0; index<numLines; index++)
	{
		if (index%2==0)
		//����������
		{
			for (j=0; j < numPixels; j++)
			{
				*(Uint8 *)(tempall + index*numPixels + j) = *(Uint8 *)(tempold + ji*numPixels + j);
				ji++;
			}
		}
		else
		//����ż����
		{
			for (j=0; j < numPixels; j++)
			{
				*(Uint8 *)(tempall + index*numPixels + j) = *(Uint8 *)(tempold + numLines/2 + ou*numPixels + j);
				ou++;
			}
		}
	}
}

void Dispicture(Uint32 tempall, Uint32 tempdis)
{
	int index;
	int ji,ou;
	int j;
	for (index = 0; index < numLines; index++)
	{
		if (index%2==0)
		//����������
		{
			for (j = 0; j < numPixels; j++)
			{
				*(Uint8 *)(tempdis + ji*numPixels + j) = *(Uint8 *)(tempall + index*numPixels + j);
				ji++;
			}
		}
		else
		//����ż����
		{
			for (j = 0; j < numPixels; j++)
			{
				*(Uint8 *)(tempdis + numLines/2 + ou*numPixels + j) = *(Uint8 *)(tempall + index*numPixels + j);
				ou++;
			}
		}
	}
}



/*ƽ��������˹ģ�壩 */
void GaussSmooth()
{
	int i,j;
	float fTemp;
	int intTemp;	
	
	for(i=intALines;i<intDLines;i++)
	{
	    for(j=intAPixels;j<intDPixels;j++) 
	    {
	    	/*��Ļ�������н��д���*/
	    	/*�ø�˹ģ����д���*/
	    	fTemp = (*(Uint8 *)(tempSrcYbuffer + (numLines/2+i-1)*numPixels + (j-1))) + 
	    	        2*(*(Uint8 *)(tempSrcYbuffer + (numLines/2+i-1)*numPixels + j)) + 
	    	        (*(Uint8 *)(tempSrcYbuffer + (numLines/2+i-1)*numPixels + (j+1))) +
	    		    2*(*(Uint8 *)(tempSrcYbuffer + i*numPixels + (j-1))) + 
	    	        4*(*(Uint8 *)(tempSrcYbuffer + i*numPixels + j)) + 
	    	        2*(*(Uint8 *)(tempSrcYbuffer + i*numPixels + (j+1))) +
                    (*(Uint8 *)(tempSrcYbuffer + (numLines/2+i)*numPixels + (j-1))) + 
	    	        2*(*(Uint8 *)(tempSrcYbuffer + (numLines/2+i)*numPixels + j)) + 
	    	        (*(Uint8 *)(tempSrcYbuffer + (numLines/2+i)*numPixels + (j+1)));
	    	  		
	  		intTemp = (int)(fTemp/16 + 0.5);
	  		
	  		if(intTemp<0)
	  		{
	  			intTemp = 0;
	  		}
	  		
	  		if(intTemp>255)
	  		{
	  			intTemp = 255;
	  		}
	    	
	    	//��Ļ�������н��д���
	    	*(Uint8 *)(tempYbuffer + i*numPixels + j) = intTemp; 
	    	
	    	/*��Ļ��ż���н��д���*/
	    	/*�ø�˹ģ����д���*/
	    	fTemp = (*(Uint8 *)(tempSrcYbuffer + i*numPixels + (j-1))) + 
	    	        2*(*(Uint8 *)(tempSrcYbuffer + i*numPixels + j)) + 
	    	        (*(Uint8 *)(tempSrcYbuffer + i*numPixels + (j+1))) +
	    	        2*(*(Uint8 *)(tempSrcYbuffer + (i+numLines/2)*numPixels + (j-1))) + 
	    	        4*(*(Uint8 *)(tempSrcYbuffer + (i+numLines/2)*numPixels + j)) + 
	    	        2*(*(Uint8 *)(tempSrcYbuffer + (i+numLines/2)*numPixels + (j+1))) +	     
	    	        (*(Uint8 *)(tempSrcYbuffer + (i+1)*numPixels + (j-1))) + 
	    	        2*(*(Uint8 *)(tempSrcYbuffer + (i+1)*numPixels + j)) + 
	    	        (*(Uint8 *)(tempSrcYbuffer + (i+1)*numPixels + (j+1))); 
	  		
	  		intTemp = (int)(fTemp/16 + 0.5);
	  		
	  		if(intTemp<0)
	  		{
	  			intTemp = 0;
	  		}
	  		
	  		if(intTemp>255)
	  		{
	  			intTemp = 255;
	  		}
	  		
	    	//��Ļ��ż���н��д���
	    	*(Uint8 *)(tempYbuffer + (i+numLines/2)*numPixels + j) = intTemp; 	    	
	    	
	    	
	    }
	}
	
			
}   

// ��ֵ�ָ�
void threshold()
{
	int i,j;
	//������������
//	for(i=intALines;i<intDLines;i++)//����
//	{
//	    for(j=intAPixels;j<intDPixels;j++) //������/ÿ��
//	    {
//	        *(Uint8 *)(tempYbuffer + i*numPixels + j) = *(Uint8 *)(tempYbuffer + i*numPixels + j)<intThreshold?0x00:0xFF;
//	    }	 
//	}
//	
//	//������ż����
//	for(i=numLines/2+intALines;i<numLines/2+intDLines;i++)//����
//	{
//	    for(j=intAPixels;j<intDPixels;j++) //������/ÿ��
//	    {		
//        	*(Uint8 *)(tempYbuffer + i*numPixels + j) = *(Uint8 *)(tempYbuffer + i*numPixels + j)<intThreshold?0x00:0xFF;   
//        }
//
//	}


int T,T0,T1,S0,N1,S1,N2,newT;
T=1,S0=0,S1=0,N1=0,N2=0,newT=0;
while(abs(T-newT)>0)
{
T=newT;
 for(i=intALines;i<intDLines;i++)//����
  {
    for(j=intAPixels;j<intDPixels;j++) //������/ÿ��
	{
	 if(*(Uint8 *)(tempYbuffer + i*numPixels + j) >=T)
	    {
			N1=N1+1;
			S0=S0+*(Uint8 *)(tempYbuffer + i*numPixels + j);
    	}
		else 
		{
			N2=N2+1;
			S1=S1+*(Uint8 *)(tempYbuffer + i*numPixels + j);			
		}	
	}
  }
  T0=S0/N1;
  T1=S1/N2;
  newT=(T0+T1)/2;
  S0=0,S1=0,N1=0,N2=0;
}   
	//������������
	for(i=intALines;i<intDLines;i++)//����
	{
	    for(j=intAPixels;j<intDPixels;j++) //������/ÿ��
	    {
	        *(Uint8 *)(tempThrebuffer + i*numPixels + j) = *(Uint8 *)(tempYbuffer + i*numPixels + j)<newT?0x00:0xFF;
			//*(Uint8 *)(tempDisYbuffer + i*numPixels + j) = tempYbuffer; 
	    }	 
	}
	
	
	//������ż����
	for(i=numLines/2+intALines;i<numLines/2+intDLines;i++)//����
	{
	    for(j=intAPixels;j<intDPixels;j++) //������/ÿ��
	    {		
        	*(Uint8 *)(tempThrebuffer + i*numPixels + j) = *(Uint8 *)(tempYbuffer+ i*numPixels + j)<newT?0x00:0xFF;   
        }

	}
//	*(Uint8 *)(tempDisYbuffer + (i+numLines/2)*numPixels + j) = intTemp; 
}

// sobel 
void sobelEdge()
{
	int i,j;
	int d1,d2,intTemp;	
	
	for(i=intALines;i<intDLines;i++)
	{
	    for(j=intAPixels;j<intDPixels;j++) 
	    {
	    	/*��Ļ�������н��д���*/
			//��������
	    	d1 = (*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + (j-1))) + 
	    	     2*(*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + j)) + 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + (j+1))) - 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i-1)*numPixels + (j-1))) - 
	    	     2*(*(Uint8 *)(tempThrebuffer + (numLines/2+i-1)*numPixels + j)) - 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i-1)*numPixels + (j+1)));
			//��������
	    	d2 = (*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + (j-1))) - 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + (j+1))) + 
	    	     2*(*(Uint8 *)(tempThrebuffer + i*numPixels + (j-1))) - 
	    	     2*(*(Uint8 *)(tempThrebuffer + i*numPixels + (j+1))) + 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i-1)*numPixels + (j-1))) - 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i-1)*numPixels + (j+1)));
	  		
	  		//d2=-d2;
	  		intTemp = d1>d2?d1:d2;
	  		
	  		if(intTemp<0)
	  		{
	  			intTemp = 0;
	  		}
	  		
	  		if(intTemp>255)
	  		{
	  			intTemp = 255;
	  		}
	    	
	    	*(Uint8 *)(tempDisYbuffer + i*numPixels + j) = intTemp; 
	    	
	    	/*��Ļ��ż���н��д���*/
			//��������
	    	d1 = (*(Uint8 *)(tempThrebuffer + (i+1)*numPixels + (j-1))) + 
	    	     2*(*(Uint8 *)(tempThrebuffer + (i+1)*numPixels + j)) + 
	    	     (*(Uint8 *)(tempThrebuffer + (i+1)*numPixels + (j+1))) - 
	    	     (*(Uint8 *)(tempThrebuffer + i*numPixels + (j-1))) - 
	    	     2*(*(Uint8 *)(tempThrebuffer + i*numPixels + j)) - 
	    	     (*(Uint8 *)(tempThrebuffer + i*numPixels + (j+1)));
			//��������
	    	d2 = (*(Uint8 *)(tempThrebuffer + (i+1)*numPixels + (j-1))) - 
	    	     (*(Uint8 *)(tempThrebuffer + (i+1)*numPixels + (j+1))) + 
	    	     2*(*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + (j-1))) - 
	    	     2*(*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + (j+1))) + 
	    	     (*(Uint8 *)(tempThrebuffer + i*numPixels + (j-1))) - 
	    	     (*(Uint8 *)(tempThrebuffer + i*numPixels + (j+1)));
	  		
	  		//d2=-d2;
	  		intTemp = d1>d2?d1:d2;
	  		
	  		if(intTemp<0)
	  		{
	  			intTemp = 0;
	  		}
	  		
	  		if(intTemp>255)
	  		{
	  			intTemp = 255;
	  		}
	    	
	    	*(Uint8 *)(tempDisYbuffer + (numLines/2+i)*numPixels + j) = intTemp; 
	    }
	}		
}   


/*Laplacian��Ե��⴦��*/
void LaplacianEdge()
{
	int i,j;
	int d1,d2,intTemp;	
	
	for(i=intALines;i<intDLines;i++)
	{
	    for(j=intAPixels;j<intDPixels;j++) 
	    {
	    	/*��Ļ�������н��д���*/
	    	d1 = -(*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + j)) - 
	    	     (*(Uint8 *)(tempThrebuffer + i*numPixels + (j+1))) + 
	    	     4*(*(Uint8 *)(tempThrebuffer + i*numPixels + j)) - 
	    	     (*(Uint8 *)(tempThrebuffer + i*numPixels + (j-1))) - 	    	     
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i-1)*numPixels + j));
	    	    
	    	d2 = -(*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + (j-1))) - 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + j)) - 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + (j+1))) - 
	    	     (*(Uint8 *)(tempThrebuffer + i*numPixels + (j-1))) + 
	    	     8*(*(Uint8 *)(tempThrebuffer + i*numPixels + j)) - 
	    	     (*(Uint8 *)(tempThrebuffer + i*numPixels + (j+1))) - 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i-1)*numPixels + (j-1))) -
	     	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i-1)*numPixels + j)) - 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i-1)*numPixels + (j+1)));
	  		
	  		intTemp = d1>d2?d1:d2;
	  		
	  		if(intTemp<0)
	  		{
	  			intTemp = 0;
	  		}
	  		
	  		if(intTemp>255)
	  		{
	  			intTemp = 255;
	  		}
	    	
	    	*(Uint8 *)(tempYbuffer + i*numPixels + j) = intTemp; 
	    	
	    	/*��Ļ��ż���н��д���*/
	    	d1 = -(*(Uint8 *)(tempThrebuffer + (i+1)*numPixels + j)) - 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + (j+1))) + 
	    	     4*(*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + j)) - 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + (j-1))) - 	    	     
	    	     (*(Uint8 *)(tempThrebuffer + i*numPixels + j));
	    	    
	    	d2 = -(*(Uint8 *)(tempThrebuffer + (i+1)*numPixels + (j-1))) - 
	    	     (*(Uint8 *)(tempThrebuffer + (i+1)*numPixels + j)) - 
	    	     (*(Uint8 *)(tempThrebuffer + (i+1)*numPixels + (j+1))) - 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + (j-1))) + 
	    	     8*(*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + j)) - 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + (j+1))) - 
	    	     (*(Uint8 *)(tempThrebuffer + i*numPixels + (j-1))) -
	     	     (*(Uint8 *)(tempThrebuffer + i*numPixels + j)) - 
	    	     (*(Uint8 *)(tempThrebuffer + i*numPixels + (j+1)));
	  		
	  		intTemp = d1>d2?d1:d2;
	  		
	  		if(intTemp<0)
	  		{
	  			intTemp = 0;
	  		}
	  		
	  		if(intTemp>255)
	  		{
	  			intTemp = 255;
	  		}
	    	
	    	*(Uint8 *)(tempYbuffer + (numLines/2+i)*numPixels + j) = intTemp; 	    	
	    	
	    }
	}				
}   

//��Ե���
float getAngle()
{
	int i,j;
	int x1,y1,x2,y2;
	int dot1,dot2;
	for (i = intALines;i < intDLines; i++)
	{
		for (j = intAPixels; j < intDPixels; j++)
		{
			if (dot1 <2 )
			{
			// �����ж�ȡ
			if(*(Uint8 *)(tempYbuffer + i*numPixels + j) == 0xFF)
			{
				x1 = j;
				y1 = i;
				dot1++;
			}
			//ż���ж�ȡ
			if(*(Uint8 *)(tempYbuffer + (numLines/2+i)*numPixels + j) == 0xFF)
			{
				x1 = j;
				y1 = i;
				dot1++;
			}
			}
			else
			{
				break;
			}
		}
	}

	for (j = intAPixels; j < intDPixels; j++)
	{
		for (i = intALines; i < intDLines; i++)
		{
			if(dot2 < 2)
			{
			if(*(Uint8 *)(tempYbuffer + i*numPixels + j) == 0xFF)
			{
				x2 = j;
				y2 = i;
				dot2++;
			}
			//ż���ж�ȡ
			if(*(Uint8 *)(tempYbuffer + (numLines/2+i)*numPixels + j) == 0xFF)
			{
				x2 = j;
				y2 = i;
				dot2++;
			}
			}
			else
			{
				break;
			}
		}
	}
	return atan2(abs(x1-x2),abs(y1-y2));
}



/*����ͼ����ת����*/
void rotate(fAngle)
{
	/*ͼ����ת����*/
	float cosAngle;   //fAngle��cosֵ
	float sinAngle;   //fAngle��sinֵ
	float f1;  //�м�ֵ
	float f2;  //�м�ֵ   	
	int i,j,intInc;
	int intCapYInc;
	int intCapX,intCapY; 
	cosAngle = cosf(fAngle);   //fAngle��cosֵ
	sinAngle = sinf(fAngle);   //fAngle��sinֵ	
	f1 = 0.5*(numPixels-1)*(1-cosAngle) - 0.5*(numLines-1)*sinAngle;
	f2 = 0.5*(numPixels-1)*sinAngle - 0.5*(numLines-1)*(1-cosAngle); 

	
	/*����ͼ����ת�����¸�ֵ*/
	//������������
	for(i=intALines;i<intDLines;i++)
	{
	    for(j=intAPixels;j<intDPixels;j++) 
	    {
            intInc = i*2;   

            intCapX    = (int)(j*cosAngle + intInc*sinAngle + f1 + 0.5);
            intCapYInc = (int)(intInc*cosAngle - j*sinAngle + f1 + 0.5);           
                      
            if((intCapYInc%2)==0)
            {
            	intCapY = intCapYInc/2;
            }
            else
            {
            	intCapY = (intCapYInc-1)/2+numLines/2;
            }
             

            //�ж��Ƿ���ԭͼ��Χ��
            if((intCapX>=0) && (intCapX<numPixels) && (intCapY>=0) && (intCapY<numLines))   
            { 
			    //���������ź�
			    *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = *(Uint8 *)(capYbuffer + intCapY*numPixels + intCapX); 		               	           	           	           	
        	} 
            else
            {
            	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0xFF;
            } 
			     
		}	 
	}
	
	
	
	//������ż����
	for(i=numLines/2+intALines;i<numLines/2+intDLines;i++)
	{
	    for(j=intAPixels;j<intDPixels;j++) 
	    {		
            intInc = (i-numLines/2)*2 + 1;

            intCapX    = (int)(j*cosAngle + intInc*sinAngle + f1 + 0.5);
            intCapYInc = (int)(intInc*cosAngle - j*sinAngle + f1 + 0.5);           
                      
            if((intCapYInc%2)==0)
            {
            	intCapY = intCapYInc/2;
            }
            else
            {
            	intCapY = (intCapYInc-1)/2+numLines/2;
            }             

            //�ж��Ƿ���ԭͼ��Χ��
            if((intCapX>=0) && (intCapX<numPixels) && (intCapY>=0) && (intCapY<numLines))   
            { 
			    //���������ź�
			    *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = *(Uint8 *)(capYbuffer + intCapY*numPixels + intCapX); 		               	           	           	           	
        	} 
            else
            {
            	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0xFF;
            } 
		              
		}	 
	}	
	
			
							
}



