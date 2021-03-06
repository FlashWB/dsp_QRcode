/********************************************************************/
/*  Copyright 2004 by SEED Incorporated.							*/
/*  All rights reserved. Property of SEED Incorporated.				*/
/*  Restricted rights to use, duplicate or disclose this code are	*/
/*  granted through contract.									    */
/*  															    */
/********************************************************************/

/********************************************************************/
/*                    图像的平滑（高斯模板）                        */ 
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
/*SEEDDM642的emifa的设置结构*/
EMIFA_Config Seeddm642ConfigA ={
	   0x00052078,/*gblctl EMIFA(B)global control register value */
	   			  /*将CLK6、4、1使能；将MRMODE置1；使能EK2EN,EK2RATE*/
	   0xffffffd3,/*cectl0 CE0 space control register value*/
	   			  /*将CE0空间设为SDRAM*/
	   0x73a28e01,/*cectl1 CE1 space control register value*/
	   			  /*Read hold: 1 clock;
	   			    MTYPE : 0000,选择8位的异步接口
	   			    Read strobe ：001110；14个clock宽度
	   			    TA：2 clock; Read setup 2 clock;
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

/*SEEDDM642的IIC的设置结构*/
I2C_Config SEEDDM642IIC_Config = {
    0,  /* master mode,  i2coar;采用主模式   */
    0,  /* no interrupt, i2cimr;只写，不读，采用无中断方式*/
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
// 地址为0 for cvbs port1,选择复合信号做为输入
Uint8 input_sel = 0x00;
/*地址为0xf，将Pin27设置成为CAPEN功能*/	
Uint8 pin_cfg = 0x02;
/*地址为1B*/
Uint8 chro_ctrl_2 = 0x14;
/*图像句柄的声明*/
VP_Handle vpHchannel0;
VP_Handle vpHchannel1;
VP_Handle vpHchannel2;

/*确定图像的参数*/
int numPixels = 720;//每行720个像素
int numLines  = 576;//每帧576行（PAL）

/*确定处理的范围*/
/*A             */
/*              */
/*             D*/ 
int intAPixels = 190;
int intALines = 59;
int intDPixels = 530;
int intDLines = 229; 

/*******************/
/*****画矩形边框函数的声明*****/
void drawRectangle();
/*平滑处理（高斯模板）*/
void GaussSmooth();

void threshold();
void Allpicture();
void Dispicture();
void sobelEdge();
void LaplacianEdge();
float getAngle();
void rotate();

/*采集与显示缓冲区的首址*/
Uint32 capYbuffer  = 0x80000000;
Uint32 capCbbuffer = 0x800675c0;
Uint32 capCrbuffer = 0x8009b0a0;

Uint32 disYbuffer  = 0x80100000;
Uint32 disCbbuffer = 0x801675c0; 
Uint32 disCrbuffer = 0x8019b0a0;

Uint32 tempSrcYbuffer    = 0x80200000;   //临时
Uint32 tempDisYbuffer    = 0x80300000;   //临时
Uint32 tempYbuffer		 = 0x80400000;   //临时存高斯处理后  //临时存边缘提取后
Uint32 tempAllbuffer	 = 0x80500000;
Uint32 tempDybuffer		 = 0x80600000;
Uint32 tempThrebuffer	 = 0x80700000;	 //临时存二值后

/*图像格式标志*/
Uint8 NTSCorPAL = 0;
extern far void vectors();
extern volatile Uint32 capNewFrame;
extern volatile Uint32 disNewFrame;

/*此程序可将四个采集口的数据经过Video Port0送出*/
void main()
{
	Uint8 addrI2C;
	int i;
	
/*-------------------------------------------------------*/
/* perform all initializations                           */
/*-------------------------------------------------------*/
	/*Initialise CSL，初始化CSL库*/
	CSL_init();
	CHIP_config(&SEEDDM642percfg);
/*----------------------------------------------------------*/
	/*EMIFA的初始化，将CE0设为SDRAM空间，CE1设为异步空间
	 注，DM642支持的是EMIFA，而非EMIF*/
	EMIFA_config(&Seeddm642ConfigA);
/*----------------------------------------------------------*/
	/*中断向量表的初始化*/
	//Point to the IRQ vector table
    IRQ_setVecs(vectors);
    IRQ_nmiEnable();
    IRQ_globalEnable();
    IRQ_map(IRQ_EVT_VINT1, 11);
    IRQ_map(IRQ_EVT_VINT0, 12);
    IRQ_reset(IRQ_EVT_VINT1);
    IRQ_reset(IRQ_EVT_VINT1);
    /*打开一个数据拷贝的数据通路*/
    DAT_open(DAT_CHAANY, DAT_PRI_LOW, DAT_OPEN_2D);	
/*----------------------------------------------------------*/	
	/*进行IIC的初始化*/
	hSeeddm642i2c = I2C_open(I2C_PORT0,I2C_OPEN_RESET);
	I2C_config(hSeeddm642i2c,&SEEDDM642IIC_Config);
/*----------------------------------------------------------*/
	/*进行TVP5150pbs的初始化*/
	/*选择TVP5150，设置第三通路*/
	GPIO_RSET(GPGC,0x0);/*将GPIO0不做为GPINT使用*/
	GPIO_RSET(GPDIR,0x1);/*将GPIO0做为输出*/
	GPIO_RSET(GPVAL,0x0);/*GPIO0输出为高，选择IIC1总线，配置
						   第二路，即为U21*/
	addrI2C = 0xBA >>1;
    _IIC_write(hSeeddm642i2c, addrI2C,0x00, input_sel);
    _IIC_write(hSeeddm642i2c, addrI2C,0x03, misc_ctrl);
    _IIC_write(hSeeddm642i2c, addrI2C,0x0D, output_format);
    _IIC_write(hSeeddm642i2c, addrI2C,0x0F, pin_cfg);
    _IIC_write(hSeeddm642i2c, addrI2C,0x1B, chro_ctrl_2);
    /*回读当前摄像设备的格式*/
    _IIC_read(hSeeddm642i2c, addrI2C,0x8c, &vFromat);
    vFromat = vFromat & 0xff;
	switch (vFromat)
	{
		case TVP51XX_NTSCM:
		case TVP51XX_NTSC443:
			NTSCorPAL = 1;/*系统为NTSC的模式*/
			break;
		case TVP51XX_PALBGHIN:
		case TVP51XX_PALM:
			NTSCorPAL = 0;/*系统为PAL的模式*/
			break;
		default:
			NTSCorPAL = 2;/*系统为不支持的模式*/
			break;
	}
	if(NTSCorPAL ==2)
	{
		/*系统不支持的模式，重新配置*/
		for(;;)
		{}
	}  		  
/*----------------------------------------------------------*/	
	/*进行SAA7121H的初始化*/
	GPIO_RSET(GPVAL,0x0);/*GPIO0输出为低，选择IIC1总线，配置图像输出*/						  
	addrI2C = 0xB8 >>1; /*选择第0路的I2C的地址*/
	/*将第0路的视频输入口的数据口设为高阻状态，
	  使能SCLK，将第27脚设为输入*/
	_IIC_write(hSeeddm642i2c, addrI2C,0x03, 0x1);
	/*配置SAA7121H*/
	GPIO_RSET(GPVAL,0x1);/*GPIO0输出为低，选择IIC1总线，配置图像输出*/	
	/*初始化Video Port0*/
	/*将Vedio Port1设为encoder输出*/
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
	/*初始化Video Port1*/
	/*将Vedio Port1设为采集输入*/
	portNumber = 1;
	vpHchannel1 = bt656_8bit_ncfc(portNumber);
	bt656_capture_start(vpHchannel1);
	/*等待第一帧数据采集完成*/
	while(capNewFrame == 0){}
	/*将数据存入显示缓冲区，并清采集完成的标志*/
	capNewFrame =0;
	for(i=0;i<numLines;i++)
	{
		/*传送临时Y缓冲区*/
		DAT_copy((void *)(capYbuffer + i * numPixels), 
	             (void *)(tempSrcYbuffer + i * numPixels),
	             numPixels);
		/*传送临时Y缓冲区*/
		DAT_copy((void *)(capYbuffer + i * numPixels), 
	             (void *)(tempDisYbuffer + i * numPixels),
	             numPixels);	             	  	 
	 }

	//合并
	Allpicture(tempAllbuffer, tempSrcYbuffer);
	/*平滑处理（高斯模板）*/
    GaussSmooth(); 
    threshold();
	LaplacianEdge();
	//Allpicture(tempYbuffer,tempAllbuffer);
	rotate(getAngle());
	//getAngle();

    /*画边框*/    
    drawRectangle();
	 
	for(i=0;i<numLines;i++)
	{
		/*传送Y缓冲区*/
		DAT_copy((void *)(tempDisYbuffer + i * numPixels), 
	             (void *)(disYbuffer + i * numPixels),
	             numPixels);
	 }	 
	
	//清除彩色信号  
	for(i=0;i<0x33ae0;i++)
	{
	    *((Uint8 *)(disCrbuffer +i)) =0x80;
	    *((Uint8 *)(disCbbuffer +i)) =0x80;

	} 
	 
	 
	/*启动显示模块*/
	bt656_display_start(vpHchannel0);
	/*建立显示的实时循环*/
	for(;;)
	{
		/*当采集区的数据已经采集好，而显示缓冲区的数据已空*/
		if((capNewFrame == 1)&&(disNewFrame == 1))
		{
			/*将数据装入显示缓冲区，并清采集完成的标志*/
			capNewFrame =0;
			disNewFrame =0;
			for(i=0;i<numLines;i++)
			{
				/*传送临时Y缓冲区*/
				DAT_copy((void *)(capYbuffer + i * numPixels), 
			             (void *)(tempSrcYbuffer + i * numPixels),
			             numPixels);
 			  	/*传送临时Y缓冲区*/
				DAT_copy((void *)(capYbuffer + i * numPixels), 
			             (void *)(tempDisYbuffer + i * numPixels),
			             numPixels);
			 }

		    //合并
			Allpicture(tempAllbuffer, tempSrcYbuffer);
			/*平滑处理（高斯模板）*/
            GaussSmooth(); 	
            threshold();
			LaplacianEdge();
			//Allpicture(tempYbuffer,tempAllbuffer);
			rotate(getAngle());
			//getAngle();

		    /*画边框*/    
		    drawRectangle();
		    			 
			for(i=0;i<numLines;i++)
			{
				/*传送Y缓冲区*/
				DAT_copy((void *)(tempDisYbuffer + i * numPixels), 
			             (void *)(disYbuffer + i * numPixels),
			             numPixels);		
			 }
		}
		
	}
	
	for(;;)
	{}
/*----------------------------------------------------------*/
	/*采集与回放*/	
}  

/*画矩形边框函数的定义*/
void drawRectangle()
{
    int i,j;
    /*画上边*/
    //奇数行
    for(i=intALines-4;i<intALines;i++)  //行数
	{
	    for(j=intAPixels-6;j<intDPixels+6;j++) //像素点数
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}	
	
	//偶数行
	for(i=numLines/2+intALines-4;i<numLines/2+intALines;i++)  //行数
	{
	    for(j=intAPixels-6;j<intDPixels+6;j++) //像素点数
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}


	/*画下边*/
	//奇数行
    for(i=intDLines;i<intDLines+4;i++)//行数
	{
	    for(j=intAPixels-6;j<intDPixels+6;j++) //像素点数
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}
	
	//偶数行
	for(i=numLines/2+intDLines;i<numLines/2+intDLines+4;i++)//行数
	{
	    for(j=intAPixels-6;j<intDPixels+6;j++) //像素点数
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}	

	
	/*画左边*/
	//奇数行
    for(i=intALines;i<intDLines;i++)//行数
	{
	    for(j=intAPixels-6;j<intAPixels;j++) //像素点数
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}
	
	//偶数行
	for(i=numLines/2+intALines;i<numLines/2+intDLines;i++)//行数
	{
	    for(j=intAPixels-6;j<intAPixels;j++) //像素点数
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}
	
	
	/*画右边*/
	//奇数行
    for(i=intALines;i<intDLines;i++)//行数
	{
	    for(j=intDPixels;j<intDPixels+6;j++) //像素点数
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}
	
	//偶数行
    for(i=numLines/2+intALines;i<numLines/2+intDLines;i++)//行数
	{
	    for(j=intDPixels;j<intDPixels+6;j++) //像素点数
	    {
	    	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0x00;
	    }
	}
} 

void Allpicture(Uint32 tempall, Uint32 tempold)
{
	//int numPixels = 720;//每行720个像素
	//int numLines  = 576;//每帧576行（PAL）
	int i,j;
	int ji=0, ou=0;
	int index;
	for (index = 0; index<numLines; index++)
	{
		if (index%2==0)
		//处理奇数行
		{
			for (j=0; j < numPixels; j++)
			{
				*(Uint8 *)(tempall + index*numPixels + j) = *(Uint8 *)(tempold + ji*numPixels + j);
				ji++;
			}
		}
		else
		//处理偶数行
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
		//处理奇数行
		{
			for (j = 0; j < numPixels; j++)
			{
				*(Uint8 *)(tempdis + ji*numPixels + j) = *(Uint8 *)(tempall + index*numPixels + j);
				ji++;
			}
		}
		else
		//处理偶数行
		{
			for (j = 0; j < numPixels; j++)
			{
				*(Uint8 *)(tempdis + numLines/2 + ou*numPixels + j) = *(Uint8 *)(tempall + index*numPixels + j);
				ou++;
			}
		}
	}
}



/*平滑处理（高斯模板） */
void GaussSmooth()
{
	int i,j;
	float fTemp;
	int intTemp;	
	
	for(i=intALines;i<intDLines;i++)
	{
	    for(j=intAPixels;j<intDPixels;j++) 
	    {
	    	/*屏幕上奇数行进行处理*/
	    	/*用高斯模板进行处理*/
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
	    	
	    	//屏幕上奇数行进行处理
	    	*(Uint8 *)(tempYbuffer + i*numPixels + j) = intTemp; 
	    	
	    	/*屏幕上偶数行进行处理*/
	    	/*用高斯模板进行处理*/
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
	  		
	    	//屏幕上偶数行进行处理
	    	*(Uint8 *)(tempYbuffer + (i+numLines/2)*numPixels + j) = intTemp; 	    	
	    	
	    	
	    }
	}
	
			
}   

// 阈值分隔
void threshold()
{
	int i,j;
	//方框内奇数行
//	for(i=intALines;i<intDLines;i++)//行数
//	{
//	    for(j=intAPixels;j<intDPixels;j++) //像素数/每行
//	    {
//	        *(Uint8 *)(tempYbuffer + i*numPixels + j) = *(Uint8 *)(tempYbuffer + i*numPixels + j)<intThreshold?0x00:0xFF;
//	    }	 
//	}
//	
//	//方框内偶数行
//	for(i=numLines/2+intALines;i<numLines/2+intDLines;i++)//行数
//	{
//	    for(j=intAPixels;j<intDPixels;j++) //像素数/每行
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
 for(i=intALines;i<intDLines;i++)//行数
  {
    for(j=intAPixels;j<intDPixels;j++) //像素数/每行
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
	//方框内奇数行
	for(i=intALines;i<intDLines;i++)//行数
	{
	    for(j=intAPixels;j<intDPixels;j++) //像素数/每行
	    {
	        *(Uint8 *)(tempThrebuffer + i*numPixels + j) = *(Uint8 *)(tempYbuffer + i*numPixels + j)<newT?0x00:0xFF;
			//*(Uint8 *)(tempDisYbuffer + i*numPixels + j) = tempYbuffer; 
	    }	 
	}
	
	
	//方框内偶数行
	for(i=numLines/2+intALines;i<numLines/2+intDLines;i++)//行数
	{
	    for(j=intAPixels;j<intDPixels;j++) //像素数/每行
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
	    	/*屏幕上奇数行进行处理*/
			//横向算子
	    	d1 = (*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + (j-1))) + 
	    	     2*(*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + j)) + 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i)*numPixels + (j+1))) - 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i-1)*numPixels + (j-1))) - 
	    	     2*(*(Uint8 *)(tempThrebuffer + (numLines/2+i-1)*numPixels + j)) - 
	    	     (*(Uint8 *)(tempThrebuffer + (numLines/2+i-1)*numPixels + (j+1)));
			//纵向算子
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
	    	
	    	/*屏幕上偶数行进行处理*/
			//横向算子
	    	d1 = (*(Uint8 *)(tempThrebuffer + (i+1)*numPixels + (j-1))) + 
	    	     2*(*(Uint8 *)(tempThrebuffer + (i+1)*numPixels + j)) + 
	    	     (*(Uint8 *)(tempThrebuffer + (i+1)*numPixels + (j+1))) - 
	    	     (*(Uint8 *)(tempThrebuffer + i*numPixels + (j-1))) - 
	    	     2*(*(Uint8 *)(tempThrebuffer + i*numPixels + j)) - 
	    	     (*(Uint8 *)(tempThrebuffer + i*numPixels + (j+1)));
			//纵向算子
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


/*Laplacian边缘检测处理*/
void LaplacianEdge()
{
	int i,j;
	int d1,d2,intTemp;	
	
	for(i=intALines;i<intDLines;i++)
	{
	    for(j=intAPixels;j<intDPixels;j++) 
	    {
	    	/*屏幕上奇数行进行处理*/
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
	    	
	    	/*屏幕上偶数行进行处理*/
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

//边缘检测
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
			// 奇数行读取
			if(*(Uint8 *)(tempYbuffer + i*numPixels + j) == 0xFF)
			{
				x1 = j;
				y1 = i;
				dot1++;
			}
			//偶数行读取
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
			//偶数行读取
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



/*进行图像旋转处理*/
void rotate(fAngle)
{
	/*图像旋转参数*/
	float cosAngle;   //fAngle的cos值
	float sinAngle;   //fAngle的sin值
	float f1;  //中间值
	float f2;  //中间值   	
	int i,j,intInc;
	int intCapYInc;
	int intCapX,intCapY; 
	cosAngle = cosf(fAngle);   //fAngle的cos值
	sinAngle = sinf(fAngle);   //fAngle的sin值	
	f1 = 0.5*(numPixels-1)*(1-cosAngle) - 0.5*(numLines-1)*sinAngle;
	f2 = 0.5*(numPixels-1)*sinAngle - 0.5*(numLines-1)*(1-cosAngle); 

	
	/*进行图像旋转，重新赋值*/
	//方框内奇数行
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
             

            //判断是否在原图范围内
            if((intCapX>=0) && (intCapX<numPixels) && (intCapY>=0) && (intCapY<numLines))   
            { 
			    //传送亮度信号
			    *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = *(Uint8 *)(capYbuffer + intCapY*numPixels + intCapX); 		               	           	           	           	
        	} 
            else
            {
            	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0xFF;
            } 
			     
		}	 
	}
	
	
	
	//方框内偶数行
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

            //判断是否在原图范围内
            if((intCapX>=0) && (intCapX<numPixels) && (intCapY>=0) && (intCapY<numLines))   
            { 
			    //传送亮度信号
			    *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = *(Uint8 *)(capYbuffer + intCapY*numPixels + intCapX); 		               	           	           	           	
        	} 
            else
            {
            	 *(Uint8 *)(tempDisYbuffer + i*numPixels + j) = 0xFF;
            } 
		              
		}	 
	}	
	
			
							
}



