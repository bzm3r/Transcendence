//	ImageChart.cpp
//
//	Generate a poster of type images.
//	Copyright (c) 2013 by Kronosaur Productions, LLC. All Rights Reserved.

#include <stdio.h>

#include <windows.h>
#include "Alchemy.h"
#include "XMLUtil.h"
#include "TransData.h"

#define STATION_TAG						CONSTLIT("Station")

#define IMAGE_VARIANT_ATTRIB			CONSTLIT("imageVariant")
#define SEGMENT_ATTRIB					CONSTLIT("segment")
#define TYPE_ATTRIB						CONSTLIT("type")
#define X_OFFSET_ATTRIB					CONSTLIT("xOffset")
#define Y_OFFSET_ATTRIB					CONSTLIT("yOffset")

struct SEntryDesc
	{
	SEntryDesc (void) : pType(NULL),
			iSize(0),
			pImage(NULL),
			iRotation(0),
			pCompositeImageArray(NULL)
		{ }

	SEntryDesc (const SEntryDesc &Src) : pType(Src.pType),
			sName(Src.sName),
			sSovereignName(Src.sSovereignName),
			iSize(Src.iSize),
			sCategorize(Src.sCategorize),
			pImage(Src.pImage),
			iRotation(Src.iRotation),
			Selector(Src.Selector)
		{
		if (Src.pCompositeImageArray)
			{
			pCompositeImageArray = new CObjectImageArray(*Src.pCompositeImageArray);
			pImage = pCompositeImageArray;
			}
		}

	~SEntryDesc (void)
		{
		if (pCompositeImageArray)
			delete pCompositeImageArray;
		}

	SEntryDesc &operator= (const SEntryDesc &Src)
		{
		if (pCompositeImageArray)
			delete pCompositeImageArray;

		pType = Src.pType;
		sName = Src.sName;
		sSovereignName = Src.sSovereignName;
		iSize = Src.iSize;
		sCategorize = Src.sCategorize;
		pImage = Src.pImage;
		iRotation = Src.iRotation;
		Selector = Src.Selector;

		if (Src.pCompositeImageArray)
			{
			pCompositeImageArray = new CObjectImageArray(*Src.pCompositeImageArray);
			pImage = pCompositeImageArray;
			}

		return *this;
		}

	CDesignType *pType;						//	The type
	CString sName;							//	Name
	CString sSovereignName;					//	Name of sovereign
	int iSize;								//	Size (units depend on design type)
	CString sCategorize;

	const CObjectImageArray *pImage;		//	Image
	int iRotation;							//	Used by ships (0 for others)

	CCompositeImageSelector Selector;		//	Used by station types
	CObjectImageArray *pCompositeImageArray;
	};

void InitStationTypeImage (SEntryDesc &Entry, CStationType *pStationType);

void GenerateImageChart (CUniverse &Universe, CXMLElement *pCmdLine)
	{
	int i;

	enum OrderTypes
		{
		orderSmallest = 1,
		orderLargest = 2,
		orderName = 3,
		orderLevel = 4,
		orderSovereign = 5,
		};

	//	Item criteria

	bool bHasItemCriteria;
	CString sCriteria;
	CItemCriteria ItemCriteria;
	if (bHasItemCriteria = pCmdLine->FindAttribute(CONSTLIT("itemCriteria"), &sCriteria))
		CItem::ParseCriteria(sCriteria, &ItemCriteria);
	else
		CItem::InitCriteriaAll(&ItemCriteria);

	//	Get the criteria from the command line.

	CDesignTypeCriteria Criteria;
	if (pCmdLine->FindAttribute(CONSTLIT("criteria"), &sCriteria))
		{
		if (CDesignTypeCriteria::ParseCriteria(sCriteria, &Criteria) != NOERROR)
			{
			printf("ERROR: Unable to parse criteria.\n");
			return;
			}
		}
	else if (bHasItemCriteria)
		{
		if (CDesignTypeCriteria::ParseCriteria(CONSTLIT("i"), &Criteria) != NOERROR)
			{
			printf("ERROR: Unable to parse criteria.\n");
			return;
			}
		}
	else
		{
		printf("ERROR: Expected criteria.\n");
		return;
		}

	bool bAll = pCmdLine->GetAttributeBool(CONSTLIT("all"));

	//	Options

	bool bTextBoxesOnly = pCmdLine->GetAttributeBool(CONSTLIT("textBoxesOnly"));
	bool bFieldUNID = pCmdLine->GetAttributeBool(CONSTLIT("unid"));

	//	Figure out what order we want

	CString sOrder = pCmdLine->GetAttribute(CONSTLIT("sort"));
	int iOrder;
	if (strEquals(sOrder, CONSTLIT("smallest")))
		iOrder = orderSmallest;
	else if (strEquals(sOrder, CONSTLIT("largest")))
		iOrder = orderLargest;
	else if (strEquals(sOrder, CONSTLIT("level")))
		iOrder = orderLevel;
	else if (strEquals(sOrder, CONSTLIT("sovereign")))
		iOrder = orderSovereign;
	else
		iOrder = orderName;

	bool bDockingPorts = pCmdLine->GetAttributeBool(CONSTLIT("portPos"));
	bool bDevicePos = pCmdLine->GetAttributeBool(CONSTLIT("devicePos"));

	//	Image size

	int cxDesiredWidth;
	if (pCmdLine->FindAttributeInteger(CONSTLIT("width"), &cxDesiredWidth))
		cxDesiredWidth = Max(512, cxDesiredWidth);
	else
		cxDesiredWidth = 1280;

	//	Spacing

	int cxSpacing = pCmdLine->GetAttributeInteger(CONSTLIT("xSpacing"));
	int cxExtraMargin = pCmdLine->GetAttributeInteger(CONSTLIT("xMargin"));
	int cxImageMargin = 2 * pCmdLine->GetAttributeInteger(CONSTLIT("xImageMargin"));

	//	Font for text

	CString sTypeface;
	int iSize;
	bool bBold;
	bool bItalic;

	if (!CG16bitFont::ParseFontDesc(pCmdLine->GetAttribute(CONSTLIT("font")),
			&sTypeface,
			&iSize,
			&bBold,
			&bItalic))
		{
		sTypeface = CONSTLIT("Arial");
		iSize = 10;
		bBold = false;
		bItalic = false;
		}

	CG16bitFont NameFont;
	NameFont.Create(sTypeface, -PointsToPixels(iSize), bBold, bItalic);
	CG32bitPixel rgbNameColor = CG32bitPixel(255, 255, 255);

	//	Rotation

	int iRotation = pCmdLine->GetAttributeInteger(CONSTLIT("rotation"));

	//	Output file

	CString sFilespec = pCmdLine->GetAttribute(CONSTLIT("output"));
	if (!sFilespec.IsBlank())
		sFilespec = pathAddExtensionIfNecessary(sFilespec, CONSTLIT(".bmp"));

	//	Generate a sorted table of types

	TSortMap<CString, SEntryDesc> Table;
	for (i = 0; i < Universe.GetDesignTypeCount(); i++)
		{
		CDesignType *pType = Universe.GetDesignType(i);
		SEntryDesc NewEntry;

		//	Make sure we match the criteria

		if (!pType->MatchesCriteria(Criteria))
			continue;

		//	Figure stuff stuff out based on the specific design type

		switch (pType->GetType())
			{
			case designItemType:
				{
				CItemType *pItemType = CItemType::AsType(pType);
				CItem Item(pItemType, 1);

				//	Skip if not in item criteria

				if (!Item.MatchesCriteria(ItemCriteria))
					continue;

				//	Skip virtual classes

				if (pItemType->IsVirtual())
					continue;

				//	Initialize the entry

				NewEntry.pType = pType;
				NewEntry.sName = pItemType->GetNounPhrase(0);
				NewEntry.pImage = &pItemType->GetImage();
				NewEntry.iSize = RectWidth(NewEntry.pImage->GetImageRect());
				break;
				}

			case designShipClass:
				{
				CShipClass *pClass = CShipClass::AsType(pType);

				//	Skip player classes, unless we want all.

				if (!bAll && pClass->IsPlayerShip())
					continue;

				//	Skip non-generic classess

				if (!bAll && !pClass->HasLiteralAttribute(CONSTLIT("genericClass")))
					continue;

				//	Initialize the entry

				NewEntry.pType = pType;
				NewEntry.sName = pClass->GetNounPhrase(0);
				NewEntry.iSize = RectWidth(pClass->GetImage().GetImageRect());
				NewEntry.pImage = &pClass->GetImage();
				NewEntry.iRotation = pClass->Angle2Direction(iRotation);
				NewEntry.sSovereignName = (pClass->GetDefaultSovereign() ? pClass->GetDefaultSovereign()->GetTypeName() : NULL_STR);
				break;
				}

			case designStationType:
				{
				CStationType *pStationType = CStationType::AsType(pType);

				//	Skip generic classes

				if (!bAll && !pStationType->HasLiteralAttribute(CONSTLIT("generic")))
					continue;

				NewEntry.pType = pType;
				NewEntry.sName = pStationType->GetNounPhrase(0);
				NewEntry.iSize = pStationType->GetSize();
				NewEntry.sSovereignName = (pStationType->GetSovereign() ? pStationType->GetSovereign()->GetTypeName() : NULL_STR);

				InitStationTypeImage(NewEntry, pStationType);

				break;
				}

			default:
				//	Don't know how to handle this type
				continue;
				break;
			}

		//	Adjust name

		if (bFieldUNID)
			NewEntry.sName = strPatternSubst(CONSTLIT("%s (%x)"), NewEntry.sName, NewEntry.pType->GetUNID());

		//	Compute the sort key

		char szBuffer[1024];
		switch (iOrder)
			{
			case orderLargest:
				wsprintf(szBuffer, "%09d%s%x",
						1000000 - NewEntry.iSize,
						NewEntry.sName.GetASCIIZPointer(),
						pType->GetUNID());
				break;

			case orderLevel:
				wsprintf(szBuffer, "%09d%s%x",
						pType->GetLevel(),
						NewEntry.sName.GetASCIIZPointer(),
						pType->GetUNID());
				break;

			case orderSmallest:
				wsprintf(szBuffer, "%09d%s%x",
						NewEntry.iSize,
						NewEntry.sName.GetASCIIZPointer(),
						pType->GetUNID());
				break;

			case orderSovereign:
				wsprintf(szBuffer, "%s|%s|%x", NewEntry.sSovereignName.GetASCIIZPointer(), NewEntry.sName.GetASCIIZPointer(), pType->GetUNID());
				NewEntry.sCategorize = NewEntry.sSovereignName;
				break;

			default:
				wsprintf(szBuffer, "%s%x", NewEntry.sName.GetASCIIZPointer(), pType->GetUNID());
				break;
			}

		//	Add to list

		Table.Insert(CString(szBuffer), NewEntry);
		}

	//	Allocate an arranger that tracks where to paint each world.

	CImageArranger Arranger;

	//	Settings for the overall arrangement

	CImageArranger::SArrangeDesc Desc;
	Desc.cxDesiredWidth = Max(512, cxDesiredWidth - (2 * (cxSpacing + cxExtraMargin)));
	Desc.cxSpacing = cxSpacing;
	Desc.cxExtraMargin = cxExtraMargin;
	Desc.pHeader = &NameFont;

	//	Generate a table of cells for the arranger

	TArray<CCompositeImageSelector> Selectors;
	Selectors.InsertEmpty(Table.GetCount());

	CString sLastCategory;
	TArray<CImageArranger::SCellDesc> Cells;
	for (i = 0; i < Table.GetCount(); i++)
		{
		SEntryDesc &Entry = Table[i];

		CImageArranger::SCellDesc *pNewCell = Cells.Insert();
		pNewCell->cxWidth = (Entry.pImage ? RectWidth(Entry.pImage->GetImageRect()) : 0) + cxImageMargin;
		pNewCell->cyHeight = (Entry.pImage ? RectHeight(Entry.pImage->GetImageRect()) : 0) + cxImageMargin;
		pNewCell->sText = Entry.sName;

		if (!strEquals(sLastCategory, Entry.sCategorize))
			{
			sLastCategory = Entry.sCategorize;
			pNewCell->bStartNewRow = true;
			}
		}

	//	Arrange

	Arranger.ArrangeByRow(Desc, Cells);

	//	Create a large image

	CG32bitImage Output;
	int cxWidth = Max(cxDesiredWidth, Arranger.GetWidth());
	int cyHeight = Arranger.GetHeight();
	Output.Create(cxWidth, cyHeight);
	printf("Creating %dx%d image.\n", cxWidth, cyHeight);

	//	Paint the images

	for (i = 0; i < Table.GetCount(); i++)
		{
		SEntryDesc &Entry = Table[i];

		int x = Arranger.GetX(i);
		int y = Arranger.GetY(i);

		//	Paint

		if (x != -1)
			{
			int xCenter = x + (Arranger.GetWidth(i) / 2);
			int yCenter = y + (Arranger.GetHeight(i) / 2);

			int xOffset;
			int yOffset;
			Entry.pImage->GetImageOffset(0, Entry.iRotation, &xOffset, &yOffset);
			int cxImage = RectWidth(Entry.pImage->GetImageRect());
			int cyImage = RectHeight(Entry.pImage->GetImageRect());

			//	Paint image

			if (!bTextBoxesOnly && Entry.pImage)
				{
				Entry.pImage->PaintImageUL(Output,
						x + (Arranger.GetWidth(i) - cxImage) / 2,
						y + (Arranger.GetHeight(i) - cyImage) / 2,
						0,
						Entry.iRotation);
				}

			//	Paint type specific stuff

			switch (Entry.pType->GetType())
				{
				case designStationType:
					{
					CStationType *pStationType = CStationType::AsType(Entry.pType);
					if (bDockingPorts)
						pStationType->PaintDockPortPositions(Output, xCenter - xOffset, yCenter - yOffset);

					if (bDevicePos)
						pStationType->PaintDevicePositions(Output, xCenter - xOffset, yCenter -yOffset);
					break;
					}
				}

			//	Paint name

			int xText = Arranger.GetTextX(i);
			int yText = Arranger.GetTextY(i);
			if (xText != -1)
				{
				if (bTextBoxesOnly)
					Output.Fill(xText, yText, Arranger.GetTextWidth(i), Arranger.GetTextHeight(i), 0xffff);

				if (!bTextBoxesOnly)
					{
					Output.FillColumn(xCenter,
							y + Arranger.GetHeight(i),
							yText - (y + Arranger.GetHeight(i)),
							rgbNameColor);

					NameFont.DrawText(Output,
							xText,
							yText,
							rgbNameColor,
							Entry.sName);
					}
				}
			}
		}

	//	Write to file or clipboard

	OutputImage(Output, sFilespec);
	}

void InitStationTypeImage (SEntryDesc &Entry, CStationType *pStationType)
	{
	struct SSatImageDesc
		{
		const CObjectImageArray *pImage;
		CCompositeImageSelector Selector;
		int xOffset;
		int yOffset;
		};

	int i;

	SSelectorInitCtx InitCtx;
	pStationType->SetImageSelector(InitCtx, &Entry.Selector);
	const CObjectImageArray *pMainImage = &pStationType->GetImage(Entry.Selector, CCompositeImageModifiers());

	//	If we have no satellites, then we can just return the single station 
	//	image.

	CXMLElement *pSatellites = pStationType->GetSatellitesDesc();
	if (pSatellites == NULL)
		{
		Entry.pImage = pMainImage;
		return;
		}

	//	Figure out the extents of the image

	RECT rcMainImage = pMainImage->GetImageRect();
	RECT rcBounds;
	rcBounds.left = -(RectWidth(rcMainImage) / 2);
	rcBounds.top = -(RectHeight(rcMainImage) / 2);
	rcBounds.right = rcBounds.left + RectWidth(rcMainImage);
	rcBounds.bottom = rcBounds.top + RectHeight(rcMainImage);

	//	Loop over all satellites and get metrics

	TArray<SSatImageDesc> SatImages;
	for (i = 0; i < pSatellites->GetContentElementCount(); i++)
		{
		CXMLElement *pSatDesc = pSatellites->GetContentElement(i);
		if (!pSatDesc->FindAttribute(SEGMENT_ATTRIB)
				|| !strEquals(STATION_TAG, pSatDesc->GetTag()))
			continue;

		//	Get the type of the satellite

		CStationType *pSatType = g_pUniverse->FindStationType(pSatDesc->GetAttributeInteger(TYPE_ATTRIB));
		if (pSatType == NULL)
			continue;

		//	Prepare the image for the satellite

		SSatImageDesc *pSatImage = SatImages.Insert();
		pSatType->SetImageSelector(InitCtx, &pSatImage->Selector);

		//	If we have an image variant, then set it

		int iVariant;
		if (pSatDesc->FindAttributeInteger(IMAGE_VARIANT_ATTRIB, &iVariant))
			{
			IImageEntry *pRoot = pSatType->GetImage().GetRoot();
			DWORD dwID = (pRoot ? pRoot->GetID() : DEFAULT_SELECTOR_ID);

			pSatImage->Selector.DeleteAll();
			pSatImage->Selector.AddVariant(dwID, iVariant);
			}

		pSatImage->pImage = &pSatType->GetImage(pSatImage->Selector, CCompositeImageModifiers());

		//	Now get the offset

		pSatImage->xOffset = pSatDesc->GetAttributeInteger(X_OFFSET_ATTRIB);
		pSatImage->yOffset = pSatDesc->GetAttributeInteger(Y_OFFSET_ATTRIB);

		//	Compute the satellite rect

		RECT rcSatImage = pSatImage->pImage->GetImageRect();
		RECT rcSatBounds;
		rcSatBounds.left = pSatImage->xOffset - (RectWidth(rcSatImage) / 2);
		rcSatBounds.top = -pSatImage->yOffset - (RectHeight(rcSatImage) / 2);
		rcSatBounds.right = rcSatBounds.left + RectWidth(rcSatImage);
		rcSatBounds.bottom = rcSatBounds.top + RectHeight(rcSatImage);

		//	Increase the size of the bounds

		rcBounds.left = Min(rcBounds.left, rcSatBounds.left);
		rcBounds.right = Max(rcBounds.right, rcSatBounds.right);
		rcBounds.top = Min(rcBounds.top, rcSatBounds.top);
		rcBounds.bottom = Max(rcBounds.bottom, rcSatBounds.bottom);
		}

	//	If no segments, then we just return the basic image

	if (SatImages.GetCount() == 0)
		{
		Entry.pImage = pMainImage;
		return;
		}

	//	Create an image that will hold the composite

	CG32bitImage *pCompositeImage = new CG32bitImage;
	pCompositeImage->Create(RectWidth(rcBounds), RectHeight(rcBounds), CG32bitImage::alpha8, CG32bitPixel::Null());
	int xCenter = -rcBounds.left;
	int yCenter = -rcBounds.top;

	//	Paint the main image

	pMainImage->PaintImage(*pCompositeImage, xCenter, yCenter, 0, Entry.iRotation, true);

	//	Paint all the satellites

	for (i = 0; i < SatImages.GetCount(); i++)
		SatImages[i].pImage->PaintImage(*pCompositeImage, xCenter + SatImages[i].xOffset, yCenter - SatImages[i].yOffset, 0, 0, true);

	//	Now create the proper image array

	RECT rcResult;
	rcResult.left = 0;
	rcResult.top = 0;
	rcResult.right = RectWidth(rcBounds);
	rcResult.bottom = RectHeight(rcBounds);

	Entry.pCompositeImageArray = new CObjectImageArray;
	Entry.pCompositeImageArray->Init(pCompositeImage, rcResult, 0, 0, true);

	//	Done

	Entry.pImage = Entry.pCompositeImageArray;
	}
