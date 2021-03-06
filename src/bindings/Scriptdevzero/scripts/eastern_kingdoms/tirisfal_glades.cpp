/* Copyright (C) 2006 - 2011 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* ScriptData
SDName: Tirisfal_Glades
SD%Complete: 100
SDComment: Quest support: 590, 1819
SDCategory: Tirisfal Glades
EndScriptData */

/* ContentData
go_mausoleum_door
go_mausoleum_trigger
npc_calvin_montague
EndContentData */

#include "precompiled.h"

/*######
## go_mausoleum_door
## go_mausoleum_trigger
######*/

enum
{
    QUEST_ULAG      = 1819,
    NPC_ULAG        = 6390,
    GO_TRIGGER      = 104593,
    GO_DOOR         = 176594
};

bool GOUse_go_mausoleum_door(Player* pPlayer, GameObject* /*pGo*/)
{
    if (pPlayer->GetQuestStatus(QUEST_ULAG) != QUEST_STATUS_INCOMPLETE)
        return false;

    if (GameObject* pTrigger = GetClosestGameObjectWithEntry(pPlayer, GO_TRIGGER, 30.0f))
    {
        pTrigger->SetGoState(GO_STATE_READY);
        pPlayer->SummonCreature(NPC_ULAG, 2390.26f, 336.47f, 40.01f, 2.26f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 300000);
        return false;
    }

    return false;
}

bool GOUse_go_mausoleum_trigger(Player* pPlayer, GameObject* pGo)
{
    if (pPlayer->GetQuestStatus(QUEST_ULAG) != QUEST_STATUS_INCOMPLETE)
        return false;

    if (GameObject* pDoor = GetClosestGameObjectWithEntry(pPlayer, GO_DOOR, 30.0f))
    {
        pGo->SetGoState(GO_STATE_ACTIVE);
        pDoor->RemoveFlag(GAMEOBJECT_FLAGS,GO_FLAG_INTERACT_COND);
        return true;
    }

    return false;
}

/*######
## npc_calvin_montague
######*/

enum
{
    SAY_COMPLETE        = -1000356,
    SPELL_DRINK         = 25700,                             // possibly not correct spell (but iconId is correct)
    QUEST_590           = 590,
    FACTION_HOSTILE     = 168
};

struct MANGOS_DLL_DECL npc_calvin_montagueAI : public ScriptedAI
{
    npc_calvin_montagueAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_uiNormFaction = pCreature->getFaction();
        Reset();
    }

    uint32 m_uiNormFaction;
    uint32 m_uiPhase;
    uint32 m_uiPhaseTimer;

    void Reset()
    {
        m_uiPhase = 0;
        m_uiPhaseTimer = 3000;

        if (m_creature->getFaction() != m_uiNormFaction)
            m_creature->setFaction(m_uiNormFaction);
    }

    void AttackedBy(Unit* pAttacker)
    {
		//Calvin has a bad habit of going friendly after you defeat him, having your last
		//hit smack him, and then going after you again.  Nip it in the bud.
        if (m_creature->getVictim() || !m_creature->IsHostileTo(pAttacker))
            return;

        AttackStart(pAttacker);
    }

	//If damage knocks us down close to death, end the fight
    void DamageTaken(Unit* /*pDoneBy*/, uint32 &uiDamage)
    {
        if (uiDamage > m_creature->GetHealth() || ((m_creature->GetHealth() - uiDamage)*100 / m_creature->GetMaxHealth() < 15))
        {
            uiDamage = 0;

			//Stop being a questgiver while we do the post-fight script event
			m_creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
			//Go friendly
            m_creature->setFaction(m_uiNormFaction);

			//Set the UI phase to 1 to start the whole post-fight script event
            m_uiPhase = 1;

			//Tell the player what killed us's group that they completed the quest
            if (Player* responsiblePlayer = m_creature->GetLootRecipient())
			{
				responsiblePlayer->GroupEventHappens(QUEST_590, m_creature);
			}
			
			//Pull us out of combat but stay in combat "mode" so we don't insta-regen health
			m_creature->RemoveAllAuras();
			m_creature->DeleteThreatList();
        }
    }

    void UpdateAI(const uint32 uiDiff)
    {
        if (m_uiPhase)
        {
			//If we have a timer, run out the timer before we start looking
			//at script triggers
            if (m_uiPhaseTimer >= uiDiff)
            {
                m_uiPhaseTimer -= uiDiff;
                return;
            }

            switch(m_uiPhase)
            {
                case 1:
					//Talk to the player and /rude
					m_creature->HandleEmote(EMOTE_ONESHOT_RUDE);
                    DoScriptText(SAY_COMPLETE, m_creature);
                    ++m_uiPhase;
					m_uiPhaseTimer = 3000;
                    break;
				case 2:
					//Start eating
					m_creature->CastSpell(m_creature,SPELL_DRINK,true);
					++m_uiPhase;
					break;
                case 3:
					//As soon as we're back to full health, stand up
					if (m_creature->GetHealthPercent() >= 100)
					{
						m_creature->SetStandState(UNIT_STAND_STATE_STAND);
						++m_uiPhase;
						m_uiPhaseTimer=3000;
					}
                    break;
				case 4:
					//Evade back to our start position
					this->ResetToHome();
					m_uiPhase = 5;
					m_uiPhaseTimer = 0;
					break;
				case 5:
					//When we're back to the start position, reset and then become a questgiver again
					if (!m_creature->IsInEvadeMode())
					{
						Reset();
						m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
					}
					break;
            }

            return;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_calvin_montague(Creature* pCreature)
{
    return new npc_calvin_montagueAI(pCreature);
}

bool QuestAccept_npc_calvin_montague(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_590)
    {
		//Start fighting the player as soon as they accept this quest
        pCreature->setFaction(FACTION_HOSTILE);
        pCreature->AI()->AttackStart(pPlayer);
    }
    return true;
}

void AddSC_tirisfal_glades()
{
    Script* pNewscript;

    pNewscript = new Script;
    pNewscript->Name = "go_mausoleum_door";
    pNewscript->pGOUse = &GOUse_go_mausoleum_door;
    pNewscript->RegisterSelf();

    pNewscript = new Script;
    pNewscript->Name = "go_mausoleum_trigger";
    pNewscript->pGOUse = &GOUse_go_mausoleum_trigger;
    pNewscript->RegisterSelf();

    pNewscript = new Script;
    pNewscript->Name = "npc_calvin_montague";
    pNewscript->GetAI = &GetAI_npc_calvin_montague;
    pNewscript->pQuestAcceptNPC = &QuestAccept_npc_calvin_montague;
    pNewscript->RegisterSelf();
}
