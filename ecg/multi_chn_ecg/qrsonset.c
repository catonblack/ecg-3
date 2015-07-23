/******************************************************************************

    函数名： qrsonset()
	语法： int qrsonset( int *buf, int maxvalue, int minvalue, int *isopoint )
	描述： 此函数是用来寻找QRS复合波的起点，在查找区间中角度最小的认为是
	       复合波起点，后面加上斜率最小者为起点
	参考论文： 主要基于以下两篇论文，论文机构一致
		   1、Electrocardiogram signal preprocessing for automatic
		      detection of QRS boundaries
		   2、Q-onset and T-end delineation: assessment of the performance
		      of an automated method with the use of a reference database
	调用： 标准库中的abs（绝对值）函数及acos（反余弦）函数
	被调用：qrsandonoffset()函数
	输入参数： qrsbuffer及当前QRS波的最大和最小值，来自qrsandonoffset()函数
	输出参数： QRS复合波起点相对于主波的偏移，及等电位点在qrsbuf中的
	           位置isopoint
	返回值： QRS复合波起点相对于主波的偏移
	其他： 本想将最大值和最小值对应的坐标也传递进来，比较两者大小，从而确定
	       最优回找时间，但考虑到较复杂，这些经验参数也不一定选取的准确，所以
		   均回溯MS120左右查找，这个通过区间办法做好
		   由于处理的信号是原始信号，而非滤波信号，信号或多或少存在干扰，所以
		   无论是最小角度还是斜率最小，效果都不会特别好，所以暂时不知，哪种
		   处理效果更好，要看以后有了前处理后比较效果
		   目前更好的是斜率和（无smooth时）
		   假如调用了平滑函数，标准必须要改
		   最小角度的理论基础还是比斜率和的好一些，加上smooth后，最小角度更好
		   终点查找是51
	 
******************************************************************************/

#include <math.h>
#include "qrsdet.h"

int qrsmooth(int *buf);

int qrsonset( int *buf, int maxvalue, int minvalue, int *isopoint )
{
	int crit = 0;//比较标准
	int loopone = 0, looptwo = 0, loopthree = 0;//三次循环的控制量（坐标）
	int conTimeOne = 0, conTimeTwo = 0;//持续时间
	int qLeft = 0, qRight = 0;//最终查找区间
	int leftMS10 = 0, rightMS10 = 0;//左（右）边MS10点，在斜率和中是左右一点
	int peakFlagone = 0, peakFlagtwo = 0;//寻找peak所需的两个变量（符号及数值）
	int slopeFlagone = 0, slopeFlagtwo = 0;//寻找slope所需的两个变量（同上）
	//角度相关变量
	double angleBuf[MS125] = {0};//角度缓存
	int angPtr = 0;//角度缓存坐标
    double bOrdY = 0, cOrdY = 0, bcOrdY = 0; //三点纵坐标差值
	double edgeA = 0, edgeB = 0, edgeC = 0;//三边长
	double angleA = 0, cosA = 0;//A的角度和余弦值
	double minAngle = 0;//最小角度为一temp值
	int minAngleLoc = 0;//最小角度坐标
	//左右斜率，寻找最小和
	int slopeOne = 0, slopeTwo = 0, slopePlus = 0;
	int slopeBuf[MS125] = {0};//斜率和缓存
	int slopePtr = 0;//斜率和缓存坐标
	int minSlope = 0;//最小斜率和为一temp值
	int minSlopeLoc = 0;//最小斜率和坐标
	
	int loc = 0;//一方面是坐标值，一方面是循环控制值
	int onsetShift = 0;//返回值，与主波的偏移量

	crit = 0.01 * (maxvalue - minvalue);//经验参数，可调

	//寻找qLeft
	//为防止QRS为错波，将条件放宽为MS110
	for (loopone = 1; loopone < MS110; ++loopone)
	{
		if (abs(buf[loopone] - buf[loopone - 1]) <= crit)
		{
			conTimeOne = conTimeOne + 1;
		}
		else
		{
			conTimeOne = 0;
		}

		//连续时间满足一定条件则记下左查找区间点
		if (conTimeOne >= MS40)
		{
			qLeft = loopone;//是否减MS15，以后要看效果
			break;
		}
	}
	if (qLeft <= 0)
	{
		qLeft = MS50;//无满足条件的点时，直接到MS50点
	}

	*isopoint = qLeft;

	//寻找qRight
	for (looptwo = qLeft; looptwo < MS125; ++looptwo)
	{
		//两条件下break，一个是peak，一个是slope
		//先peak
		leftMS10 = looptwo - MS10;
		rightMS10 = looptwo + MS10;
		if (leftMS10 < 0)
		{
			leftMS10 = 0;
		}
		//相差的纵坐标值
		peakFlagone = buf[looptwo] - buf[leftMS10];
		peakFlagtwo = buf[looptwo] - buf[rightMS10];
		if (peakFlagone * peakFlagtwo > 0)
		{
			peakFlagone = abs(peakFlagone);
			peakFlagtwo = abs(peakFlagtwo);
			//标准可改，最初是2*crit
			if ((peakFlagone >= 2 * crit) &&(peakFlagtwo >= 2 * crit))
			{
				qRight = looptwo;
				break;
			}
		}

		//再slope
		//相差的纵坐标值
		slopeFlagone = buf[looptwo + 1] - buf[looptwo];
		slopeFlagtwo = buf[looptwo + 2] - buf[looptwo + 1];
		//这里的方法与查找qLeft类似
		if (slopeFlagone * slopeFlagtwo > 0)
		{
			slopeFlagone = abs(slopeFlagone);
			slopeFlagtwo = abs(slopeFlagtwo);
			//标准可改，最初是2*crit
			if ((slopeFlagone >= 2 * crit) && (slopeFlagtwo >= 2 * crit))
			{
				conTimeTwo = conTimeTwo + 1;
			}
			else
			{
				conTimeTwo = 0;
			}
			if (conTimeTwo >= MS20)
			{
				qRight = looptwo - MS20;
				break;
			}
		}
	}
	if (qRight == 0)
	{
		qRight = MS100;//无满足条件的点时，直接到MS100点
	}

	if (qLeft >= qRight)
	{
		onsetShift = MS125 - qLeft;
		return(onsetShift);
	}

	//qrsmooth(buf);
	qrsmooth(buf);
	qrsmooth(buf);
	qrsmooth(buf);
	qrsmooth(buf);

	//可考虑找角度最小点或斜率最小点，看一下那个更准确和更好调试
	//找角度最小点
	for (loopthree = qLeft; loopthree <= qRight; ++loopthree)
	{
		leftMS10 = loopthree - MS20;//是否减去MS20更准确？
		rightMS10 = loopthree + MS20;
		if (leftMS10 < 0)
		{
			leftMS10 = 0;
		}
		if (loopthree == leftMS10)
		{
			angleBuf[angPtr] = -1;
		    ++angPtr;
			continue;
		}

		//计算纵坐标和边长
		bOrdY = buf[rightMS10] - buf[loopthree];
		cOrdY = buf[leftMS10] - buf[loopthree];
		bcOrdY = buf[rightMS10] - buf[leftMS10];
		bcOrdY = 4 + bcOrdY * bcOrdY;
		edgeA = sqrt(bcOrdY);
		cOrdY = 1 + cOrdY * cOrdY;
		edgeB = sqrt(cOrdY);
		bOrdY = 1 + bOrdY * bOrdY;
		edgeC = sqrt(bOrdY);
		//利用余弦定理计算余弦值
		cosA = (edgeB * edgeB + edgeC * edgeC - edgeA * edgeA)
			                                   /(2 * edgeB * edgeC);
		angleA = acos(cosA);
		angleBuf[angPtr] = angleA;
		++angPtr;
	}

	minAngle = angleBuf[0];
	minAngleLoc = 0;
	angPtr = angPtr - MS10;
	angPtr = (angPtr > 0) ? angPtr : 1;
	if (angPtr > 2)
	{
		for (loc = 2; loc < angPtr; ++loc)
		{
			if (minAngle > angleBuf[loc])
			{
				minAngle = angleBuf[loc];
				minAngleLoc = loc;
			}
		}
	}
	else
	{
		minAngleLoc = 0;
	}

	minAngleLoc = minAngleLoc + qLeft;
	//minAngleLoc = qRight;//调试用

	onsetShift = MS125 - minAngleLoc;
	return(onsetShift);
	
	////找左右斜率和最小点，直接利用已有的一些变量
	//for (loopthree = qLeft; loopthree <= qRight; ++loopthree)
	//{
	//	leftMS10 = loopthree - 1;
	//	rightMS10 = loopthree + 1;
	//	if (leftMS10 < 0)
	//	{
	//		leftMS10 = 0;
	//	}
	//	slopeOne = abs(buf[loopthree] - buf[leftMS10]);
	//	slopeTwo = abs(buf[rightMS10] - buf[loopthree]);
	//	slopePlus = slopeOne + slopeTwo;
	//	slopeBuf[slopePtr] = slopePlus;
	//	++slopePtr;
	//}
	//minSlope = slopeBuf[0];
	//minSlopeLoc = 0;

	//for (loc = 0; loc < slopePtr; ++loc)
	//{
	//	if (minSlope > slopeBuf[loc])
	//	{
	//		minSlope = slopeBuf[loc];
	//		minSlopeLoc = loc;
	//	}
	//}

	//minSlopeLoc = minSlopeLoc + qLeft;
	////minSlopeLoc = qLeft;//调试用

	//onsetShift = MS125 - minSlopeLoc;
	//return(onsetShift);

}