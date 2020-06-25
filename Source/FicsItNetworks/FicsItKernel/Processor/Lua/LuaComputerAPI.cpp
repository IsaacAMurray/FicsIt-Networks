#include "LuaComputerAPI.h"


#include "FGTimeSubsystem.h"
#include "FINStateEEPROMLua.h"
#include "LuaInstance.h"
#include "LuaProcessor.h"

#define LuaFunc(funcName) \
int funcName(lua_State* L) { \
	KernelSystem* kernel = LuaProcessor::luaGetProcessor(L)->getKernel();


namespace FicsItKernel {
	namespace Lua {
		LuaFunc(luaComputerGetInstance)
			newInstance(L, Network::NetworkTrace(kernel->getNetwork()->component));
			return LuaProcessor::luaAPIReturn(L, 1);
		}

#pragma optimize("", off)
		LuaFunc(luaComputerReset)
			kernel->reset();
			lua_yield(L, 0);
			return 0;
		}

		LuaFunc(luaComputerStop)
			kernel->stop();
			lua_yield(L, 0);
			return 0;
		}
#pragma optimize("", on)

		LuaFunc(luaComputerBeep)
			// TODO: do the beep
			return LuaProcessor::luaAPIReturn(L, 0);
		}

		LuaFunc(luaComputerSetEEPROM)
			AFINStateEEPROMLua* eeprom = static_cast<LuaProcessor*>(kernel->getProcessor())->getEEPROM();
			if (!IsValid(eeprom)) return luaL_error(L, "no eeprom set");
			eeprom->Code = luaL_checkstring(L, 1);
			return 0;
		}

		LuaFunc(luaComputerGetEEPROM)
            AFINStateEEPROMLua* eeprom = static_cast<LuaProcessor*>(kernel->getProcessor())->getEEPROM();
			if (!IsValid(eeprom)) return luaL_error(L, "no eeprom set");
			lua_pushlstring(L, TCHAR_TO_UTF8(*eeprom->Code), eeprom->Code.Len());
			return 1;
		}

		LuaFunc(luaComputerTime)
			AFGTimeOfDaySubsystem* subsys = AFGTimeOfDaySubsystem::Get(kernel->getNetwork()->getComponent());
			lua_pushnumber(L, subsys->GetPassedDays() * 86400 + subsys->GetDaySeconds());
			return 1;
		}

		static const luaL_Reg luaComputerLib[] = {
			{"getInstance", luaComputerGetInstance},
			{"reset", luaComputerReset},
			{"stop", luaComputerStop},
			{"beep", luaComputerBeep},
			{"setEEPROM", luaComputerSetEEPROM},
			{"getEEPROM", luaComputerGetEEPROM},
			{"time", luaComputerTime},
			{NULL,NULL}
		};
		
		void setupComputerAPI(lua_State* L) {
			PersistSetup("Computer", -2);
			luaL_newlibtable(L, luaComputerLib);
			luaL_setfuncs(L, luaComputerLib, 0);
			PersistTable("Lib", -1);
			lua_setglobal(L, "computer");
		}
	}
}