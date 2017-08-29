//	CArmorSystem.cpp
//
//	CArmorSystem class
//	Copyright (c) 2016 by Kronosaur Productions, LLC. All Rights Reserved.

#include "PreComp.h"

int CArmorSystem::CalcTotalHitPoints (CSpaceObject *pSource, int *retiMaxHP) const

//	CalcTotalHitPoints
//
//	Computes total hit points (and max hit points) for all armor segments.

	{
	int i;

	int iTotalHP = 0;
	int iMaxHP = 0;

	for (i = 0; i < m_Segments.GetCount(); i++)
		{
		iTotalHP += m_Segments[i].GetHitPoints();
		iMaxHP += m_Segments[i].GetMaxHP(pSource);
		}

	if (retiMaxHP)
		*retiMaxHP = iMaxHP;

	return iTotalHP;
	}

void CArmorSystem::Install (CSpaceObject *pObj, const CShipArmorDesc &Desc, bool bInCreate)

//  Install
//
//  Install armor segments

    {
	DEBUG_TRY

    int i;
    ASSERT(m_Segments.GetCount() == 0);

    //  We insert armor segments too

    CItemListManipulator Items(pObj->GetItemList());

    //  Loop over all sections

	m_Segments.InsertEmpty(Desc.GetCount());
	for (i = 0; i < Desc.GetCount(); i++)
		{
        const CShipArmorSegmentDesc &Sect = Desc.GetSegment(i);

		//	Create an armor segment item

        CItem ArmorItem;
        Sect.CreateArmorItem(&ArmorItem);

        //  Add the item. We leave the cursor on the newly created item.

		Items.AddItem(ArmorItem);

		//	Install

		m_Segments[i].Install(pObj, Items, i, bInCreate);
		}

    //  Initialize other properties

    m_iHealerLeft = 0;

	DEBUG_CATCH
    }

void CArmorSystem::ReadFromStream (SLoadCtx &Ctx, CSpaceObject *pObj)

//  ReadFromStream
//
//  Read from stream

    {
    int i;

	DWORD dwCount;
	Ctx.pStream->Read((char *)&dwCount, sizeof(DWORD));
	m_Segments.InsertEmpty(dwCount);
	for (i = 0; i < (int)dwCount; i++)
		m_Segments[i].ReadFromStream(pObj, i, Ctx);

    //  Read other properties

    if (Ctx.dwVersion >= 128)
        Ctx.pStream->Read((char *)&m_iHealerLeft, sizeof(DWORD));
    else
        m_iHealerLeft = 0;
    }

bool CArmorSystem::RepairAll (CSpaceObject *pSource)

//	RepairAll
//
//	Makes sure all armor segments are at full hit points. Returns TRUE if we
//	repaired anything.

	{
	int i;

	bool bRepaired = false;
	for (i = 0; i < m_Segments.GetCount(); i++)
		{
		CInstalledArmor &Armor = GetSegment(i);
		int iHP = Armor.GetHitPoints();
		int iMaxHP = Armor.GetMaxHP(pSource);

		if (iHP < iMaxHP)
			{
			Armor.SetHitPoints(iMaxHP);
			bRepaired = true;
			}
		}

	return true;
	}

bool CArmorSystem::RepairSegment (CSpaceObject *pSource, int iSeg, int iHPToRepair, int *retiHPRepaired)

//	RepairSegment
//
//	Repairs the given number of hit points on the segment. Returns TRUE if we
//	repaired anything.
//
//	NOTE: If iHPToRepair is negative, we repair all hit points.

	{
	if (iSeg < 0 || iSeg >= m_Segments.GetCount())
		{
		if (retiHPRepaired) *retiHPRepaired = 0;
		return false;
		}

	CInstalledArmor &Armor = GetSegment(iSeg);
	int iHP = Armor.GetHitPoints();
	int iMaxHP = Armor.GetMaxHP(pSource);

	//	Compute the new HP for the segment. If iHPToRepair is negative, then we
	//	repair all damage.

	int iNewHP;
	if (iHPToRepair < 0)
		iNewHP = iMaxHP;
	else
		iNewHP = Min(iHP + iHPToRepair, iMaxHP);

	//	Return how many HP we repaired

	if (retiHPRepaired)
		*retiHPRepaired = iNewHP - iHP;

	//	If nothing to do, nothing to do.

	if (iNewHP == iHP)
		return false;

	Armor.SetHitPoints(iNewHP);
	return true;
	}

void CArmorSystem::SetTotalHitPoints (CSpaceObject *pSource, int iNewHP)

//	SetTotalHitPoints
//
//	Sets the hit points for the total system, distributing as appropriate.

	{
	int i;

	//	First figure out some totals

	int iTotalMaxHP = 0;
	int iTotalHP = CalcTotalHitPoints(pSource, &iTotalMaxHP);

	//	Compute the delta

	iNewHP = Max(0, Min(iNewHP, iTotalMaxHP));
	int iDeltaHP = iNewHP - iTotalHP;
	if (iDeltaHP == 0)
		return;

	//	Slightly different algorithms for healing vs. destroying.

	if (iDeltaHP > 0)
		{
		int iHPLeft = iDeltaHP;
		int iTotalHPNeeded = iTotalMaxHP - iTotalHP;

		for (i = 0; i < m_Segments.GetCount(); i++)
			{
			CInstalledArmor &Armor = GetSegment(i);
			int iHP = Armor.GetHitPoints();
			int iMaxHP = Armor.GetMaxHP(pSource);

			//	To each according to their need
			//
			//	NOTE: iTotalHPNeeded cannot be 0 because that would imply that iTotalHP
			//	equals iTotalMaxHP. But if that were the case, iDeletaHP would be 0, so
			//	we wouldn't be in this code path.

			int iHPNeeded = iMaxHP - iHP;
			int iHPToHeal = Min(iHPLeft, iDeltaHP * iHPNeeded / iTotalHPNeeded);

			//	Heal

			Armor.SetHitPoints(iHP + iHPToHeal);

			//	Keep track of how much we've used up.

			iHPLeft -= iHPToHeal;
			}

		//	If we've got extra hit points, then distribute around.

		for (i = 0; i < m_Segments.GetCount() && iHPLeft > 0; i++)
			{
			CInstalledArmor &Armor = GetSegment(i);
			int iHP = Armor.GetHitPoints();
			int iMaxHP = Armor.GetMaxHP(pSource);

			if (iHP < iMaxHP)
				{
				Armor.SetHitPoints(iHP + 1);
				iHPLeft--;
				}
			}
		}
	else
		{
		int iDamageLeft = -iDeltaHP;
		
		for (i = 0; i < m_Segments.GetCount(); i++)
			{
			CInstalledArmor &Armor = GetSegment(i);
			int iHP = Armor.GetHitPoints();
			int iMaxHP = Armor.GetMaxHP(pSource);

			//	Damage in proportion to HP left.
			//
			//	NOTE: iTotalHP cannot be 0 because that would imply that iDeltaHP is
			//	non-negative, in which case we would not be on this code path.

			int iHPToDamage = Min(iDamageLeft, -iDeltaHP * iHP / iTotalHP);

			//	Damage

			Armor.SetHitPoints(iHP - iHPToDamage);

			//	Keep track of how much damage we've used up

			iDamageLeft -= iHPToDamage;
			}

		//	If we've got extra damage, then distribute

		for (i = 0; i < m_Segments.GetCount() && iDamageLeft > 0; i++)
			{
			CInstalledArmor &Armor = GetSegment(i);
			int iHP = Armor.GetHitPoints();

			if (iHP > 0)
				{
				Armor.SetHitPoints(iHP - 1);
				iDamageLeft--;
				}
			}
		}
	}

bool CArmorSystem::Update (SUpdateCtx &Ctx, CSpaceObject *pSource, int iTick)

//	Update
//
//	Must be called once per tick to update the armor system. We return TRUE if 
//	the update modified the properties of the armor (e.g., hit points).

	{
	int i;

	//	We only update armor once every 10 ticks.

    if ((iTick % CArmorClass::TICKS_PER_UPDATE) != 0)
		return false;

	//	Update all segments.

	bool bSystemModified = false;
    for (i = 0; i < GetSegmentCount(); i++)
        {
        CInstalledArmor *pArmor = &GetSegment(i);

		bool bArmorModified;
        pArmor->GetClass()->Update(CItemCtx(pSource, pArmor), Ctx, iTick, &bArmorModified);
        if (bArmorModified)
            bSystemModified = true;
        }

	//	Done

	return bSystemModified;
	}

void CArmorSystem::WriteToStream (IWriteStream *pStream)

//  WriteToStream
//
//  Write

    {
    int i;

    DWORD dwSave = m_Segments.GetCount();
	pStream->Write((char *)&dwSave, sizeof(DWORD));
	for (i = 0; i < m_Segments.GetCount(); i++)
		m_Segments[i].WriteToStream(pStream);

    //  Write other properties

	pStream->Write((char *)&m_iHealerLeft, sizeof(DWORD));
    }
