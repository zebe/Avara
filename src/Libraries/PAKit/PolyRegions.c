/*
    Copyright �1993-1996, Juri Munkki
    All rights reserved.

    File: PolyRegions.c
    Created: Sunday, September 12, 1993, 21:55
    Modified: Wednesday, February 7, 1996, 19:15
*/

#pragma options(!profile)

#include "PAKit.h"

extern	PolyWorld					*thePolyWorld;
static	Point						coordinateConvert;

#define	REGIONSECTIONCOUNT	128

/*
**	A vertical line segment adder that uses a special coordinate system
**	defined by coordinateConvert. For internal use only! (That's why it's static)
*/
static
void	AddVerticalPolySegment(
	short	x,
	short	y1,
	short	y2)
{
asm	{
		movem.l	A2/D2-D3,-(sp)
		
		move.l	thePolyWorld,A0
		move.w	x,D0
		sub.w	coordinateConvert.h,D0
		cmp.w	OFFSET(PolyWorld,bounds.right)(A0),D0
		bge		@noEdge						; Line is totally on right side of clip bound

		move.w	y1,D2
		sub.w	coordinateConvert.v,D2
		move.w	y2,D3
		sub.w	coordinateConvert.v,D3
		cmp.w	D2,D3
		beq		@noEdge
		bge.s	@noSwap
		exg		D2,D3
@noSwap

		move.l	OFFSET(PolyWorld,newEdge)(A0),A1
		cmp.l	OFFSET(PolyWorld,endEdge)(A0),A1
		bcc		@noEdge
		
		swap	D0
		clr.w	D0
		move.w	D3,D1
		sub.w	D2,D1							; D1 = delta y
		clr.l	PolyEdge.dx(A1)			; Store slope 
		
		ext.l	D2
		bpl.s	@noClip
		clr.w	D2
@noClip
		move.l	D0,PolyEdge.x(A1)

		move.l	OFFSET(PolyWorld,onLists)(A0),A2
		move.l	(A2,D2.w*4),D0
		move.l	A1,(A2,D2.w*4)
		move.l	D0,PolyEdge.nextOn(A1)

		cmp.w	OFFSET(PolyWorld,height)(A0),D3
		bge.s	@noOffList

		move.l	OFFSET(PolyWorld,offLists)(A0),A2
		move.l	-4(A2,D3.w*4),D0
		move.l	A1,-4(A2,D3.w*4)
		move.l	D0,PolyEdge.nextOff(A1)
@noOffList
		move.w	PolyWorld.currentColor(A0),PolyEdge.color(A1)
		move.w	OFFSET(PolyWorld,polyCount)(A0),PolyEdge.polyID(A1)
		add.l	#sizeof(PolyEdge),OFFSET(PolyWorld,newEdge)(A0)
@noEdge
		movem.l	(sp)+,A2/D2-D3
	}
}

void	PunchRegionHole(
	RgnHandle	theRegion,
	Rect		*theArea,
	short		inversionFlag)
{
#ifdef THINK_C
	#if __option(a4_globals)
		#define	regPtr	A5
		#define	SAVEA5
	#endif
#endif

				Point	aList[REGIONSECTIONCOUNT], bList[REGIONSECTIONCOUNT];
				Point	*readStart, *writeStart;
	register	Point	*readList, *writeList;
	register	short	currentY;
	register	short	activePoint;
	register	short	regionPoint;
#ifndef regPtr
	register	short	*regPtr;
#else
				Ptr		savedA5;
#endif
				Rect	area;
	
	area = *theArea;
	
	asm	{
#ifdef SAVEA5
			move.l	A5,savedA5
#endif
			lea		aList, readList
			lea		bList, writeList

			move.l	readList, readStart
			move.l	writeList, writeStart

			move.l	theRegion, regPtr
			move.l	(regPtr),regPtr
			addq.l	#2,regPtr				//	Skip region size word
			
			move.w	(regPtr)+,currentY		//	Get top row Y
			cmp.w	area.top, currentY
			bge.s	@noInitialClipTop
			move.w	area.top, currentY
@noInitialClipTop
			swap.w	currentY				//	Move it to currentY.high
			move.w	(regPtr)+, currentY		//	left bound of region
											//	topLeft is now in currentY.long

			move.w	inversionFlag,D1
			bne.s	@noInversion
			move.l	currentY,(readList)+	//	write topLeft to initia inversion point list
@noInversion
			move.w	#32767,currentY			//	"topRight" is now in currentY
			move.l	currentY,(readList)+	//	write to initial inversion point list
			lea		aList, readList			//	load bList into readList again
			addq.l	#4,regPtr				//	skip bottom right coordinates of region
@startLine
			move.w	(regPtr)+,currentY		//	Load a new Y from the region
			cmp.w	area.bottom, currentY	//	Check to see if we are beyond interesting area
			bge.s	@regionEnded			//	(stop, if we are)
			cmp.w	area.top, currentY		//	Check against top of area
			bge.s	@noClipTop
			move.w	area.top, currentY		//	currentY was above top, move to top.
@noClipTop
			move.w	currentY,regionPoint
			swap.w	regionPoint				//	Prepare regionPoint for X coordinates
@readMore
			move.l	(readList)+,activePoint	//	Read a point from the list
@regionRead
			move.w	(regPtr)+,regionPoint	//	Read the next X coordinate into regionPoint
			cmp.w	area.left,regionPoint	//	Is X to the left of the area?
			bge.s	@noClipLeft
			move.w	area.left,regionPoint	//	X was on the left side, move to area.left
@noClipLeft
@compareThem
			cmp.w	activePoint,regionPoint		//	Compare points
			bge.s	@notNewPoint				//	New point should be inserted onto list
			move.l	regionPoint,(writeList)+	//	Write region point into active list
			bra.s	@regionRead					//	Read a new point from the region
@notNewPoint
			beq.s	@outSegment					//	Points were equal, line segment time...
			move.l	activePoint,(writeList)+	//	Active point was on left side of X
			move.l	(readList)+,activePoint		//	So we wrote it out and read a new one
			bra.s	@compareThem				//	Compare this one with the region data point
@outSegment
			cmp.w	#32767,regionPoint			//	Did we reach the right edge?
			beq.s	@endLine
			
			move.w	currentY,-(sp)				//	This is where the line ends
			swap.w	activePoint					//	X and Y swapped for stack
			move.l	activePoint,-(sp)			//	Move x and y into stack
			jsr		AddVerticalPolySegment		//	Draw the segment
			addq.l	#6,sp						//	C style pop of arguments off the stack
			bra.s	@readMore					//	Back to work...
@endLine
			move.l	regionPoint,(writeList)+	//	End of line, write regionPoint out to end list.
			move.l	readStart,writeList			//	Swap readStart and writeStart
			move.l	writeStart,readList			//	And: re-initialize readStart and writeStart
			move.l	readList,readStart
			move.l	writeList,writeStart
			
			bra.s	@startLine					//	Go to next line
@writeFinals
			move.w	area.bottom,D0				//	We write out the current inversion list
			move.w	D0,-(sp)
			swap.w	activePoint
			move.l	activePoint,-(sp)
			jsr		AddVerticalPolySegment
			addq.l	#6,sp
@regionEnded
			move.l	(readList)+,activePoint		//	Load inversion list item to activePoint
			cmp.w	#32767,activePoint			//	Was there an item on the inversion list?
			bne.s	@writeFinals				//	If there was, we add a line segment.

#ifdef SAVEA5
			move.l	savedA5,A5
#endif
		}
}

/*
**	Since we work in a Macintosh environment, we have to support windows.
**	Most of the time the visible region of a window that we are animating
**	is simply a rectangle, so that case is handled relatively quickly. If
**	the region is both rectangular and larger than the clipping rectangle,
**	no additional work will be done.
**
**	If the visible region should limit the drawing in some way, create a
**	clipping polygon that masks out the areas that should not be drawn into.
*/

void	PolygonizeVisRegion(
	RgnHandle	theRegion)
{
	short	savedPolyCount;
	short	savedPolyColor;
	short	state;
	
				Rect	visBox;
	register	Rect	*regBox;
				Rect	intersection;
	
	visBox = thePolyWorld->bounds;
	
	coordinateConvert.h = 0;
	coordinateConvert.v = 0;
	GlobalToLocal(&coordinateConvert);
	visBox.left += coordinateConvert.h;
	visBox.right += coordinateConvert.h;
	visBox.top += coordinateConvert.v;
	visBox.bottom += coordinateConvert.v;
	
	coordinateConvert.v += thePolyWorld->bounds.top;

	state = HGetState((Handle)theRegion);
	HLock((Handle)theRegion);
	
//	savedPolyCount = thePolyWorld->polyCount;
//	savedPolyColor = thePolyWorld->currentColor;
	thePolyWorld->polyCount++;// = 32767;
	thePolyWorld->currentColor = -1;

	regBox = &((*theRegion)->rgnBBox);
	
	if(QSectRect(regBox, &visBox, &intersection))
	{	
		//	Is the region on the right side of left clipping edge?
		if(intersection.left > visBox.left)
		{	AddVerticalPolySegment(visBox.left, visBox.top, visBox.bottom);
		}
		else
		{	//	Mask off top area?
			if(visBox.top < intersection.top)
				AddVerticalPolySegment(visBox.left, visBox.top, intersection.top);
	
			//	Mask off bottom area?
			if(visBox.bottom > intersection.bottom)
				AddVerticalPolySegment(visBox.left, intersection.bottom, visBox.bottom);
		}
		
		if((*theRegion)->rgnSize != 10)	//	Is it a complicated region?
		{	PunchRegionHole(theRegion, &intersection, intersection.left > visBox.left);
		}
		else
		{	//	Intersection of a rectangle and a rectangle is a rectangle!
			//	Now we simply make a rectangular hole:
		
			if(intersection.left > visBox.left)
				AddVerticalPolySegment(intersection.left, intersection.top, intersection.bottom);

			if(intersection.right < visBox.right)
				AddVerticalPolySegment(intersection.right, intersection.top, intersection.bottom);
		}
	}
	else
	{	//	Empty visRegion, clip everything.	
		AddVerticalPolySegment(visBox.left, visBox.top, visBox.bottom);
	}

//	thePolyWorld->polyCount = savedPolyCount;
//	thePolyWorld->currentColor = savedPolyColor;

	HSetState((Handle)theRegion,state);
}