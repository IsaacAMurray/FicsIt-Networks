﻿#pragma once

#include "FicsItNetworksModule.h"
#include "Network/FINHookSubsystem.h"
#include "FINReflection.h"
#include "FINSignal.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "FGPowerCircuit.h"
#include "FGTrain.h"
#include "Buildables/FGBuildablePipeHyper.h"
#include "Buildables/FGBuildableRailroadSignal.h"
#include "Buildables/FGPipeHyperStart.h"
#include "Patching/NativeHookManager.h"
#include "FGCharacterMovementComponent.h"
#include "FGCharacterPlayer.h"
#include "FGLocomotive.h"
#include "Buildables/FGBuildableRailroadStation.h"
#include "FINStaticReflectionSourceHooks.generated.h"
#include "FGDroneVehicle.h"
#include "Buildables/FGBuildableDroneStation.h"

UCLASS()
class FICSITNETWORKS_API UFINStaticReflectionHook : public UFINHook {
	GENERATED_BODY()

protected:
	UPROPERTY()
	UFINSignal* Signal = nullptr;

protected:
	UPROPERTY()
	UObject* Sender = nullptr;

protected:
	void Send(const TArray<FFINAnyNetworkValue>& Values) {
		Signal->Trigger(Sender, Values);
	}

public:
	void Register(UObject* sender) override {
		Super::Register(sender);

		Sender = sender;
	}
};

UCLASS()
class FICSITNETWORKS_API UFINFunctionHook : public UFINHook {
	GENERATED_BODY()

private:
	UPROPERTY()
	UObject* Sender;
	
	bool bIsRegistered;

	UPROPERTY()
	TMap<FString, UFINSignal*> Signals;

protected:
	UPROPERTY()
	TSet<TWeakObjectPtr<UObject>> Senders;
	FCriticalSection Mutex;
	
	bool IsSender(UObject* Obj) {
		FScopeLock Lock(&Mutex);
		return Senders.Contains(Obj);
	}
	
	void Send(UObject* Obj, const FString& SignalName, const TArray<FFINAnyNetworkValue>& Data) {
		UFINSignal** Signal = Signals.Find(SignalName);
		if (!Signal) {
			UFINClass* Class = FFINReflection::Get()->FindClass(Obj->GetClass());
			UFINSignal* NewSignal = Class->FindFINSignal(SignalName);
			if (!NewSignal) UE_LOG(LogFicsItNetworks, Error, TEXT("Signal with name '%s' not found for object '%s' of FINClass '%s'"), *SignalName, *Obj->GetName(), *Class->GetInternalName());
			Signal = &Signals.Add(SignalName, NewSignal);
		}
		if (Signal) (*Signal)->Trigger(Obj, Data);
	}

	virtual void RegisterFuncHook() {}

	virtual UFINFunctionHook* Self() { return nullptr; }

public:		
	void Register(UObject* sender) override {
		Super::Register(sender);
		
		FScopeLock Lock(&Self()->Mutex);
    	Self()->Senders.Add(Sender = sender);

		if (!Self()->bIsRegistered) {
			Self()->bIsRegistered = true;
			Self()->RegisterFuncHook();
		}
    }
		
	void Unregister() override {
		FScopeLock Lock(&Self()->Mutex);
    	Self()->Senders.Remove(Sender);
    }
};

UCLASS()
class FICSITNETWORKS_API UFINMultiFunctionHook : public UFINHook {
	GENERATED_BODY()

private:
	UPROPERTY()
	UObject* Sender;
	
	bool bIsRegistered;

	UPROPERTY()
	TMap<FString, UFINSignal*> Signals;

protected:
	UPROPERTY()
	TSet<TWeakObjectPtr<UObject>> Senders;
	FCriticalSection Mutex;
	
	bool IsSender(UObject* Obj) {
		FScopeLock Lock(&Mutex);
		return Senders.Contains(Obj);
	}
	
	void Send(UObject* Obj, const FString& SignalName, const TArray<FFINAnyNetworkValue>& Data) {
		UFINSignal* Signal;
		if (Signals.Contains(SignalName)) {
			Signal = Signals[SignalName];
		}else{
			UFINClass* Class = FFINReflection::Get()->FindClass(Obj->GetClass());
			Signal = Class->FindFINSignal(SignalName);
			if (!Signal) UE_LOG(LogFicsItNetworks, Error, TEXT("Signal with name '%s' not found for object '%s' of FINClass '%s'"), *SignalName, *Obj->GetName(), *Class->GetInternalName());
			Signals.Add(SignalName, Signal);
		}
		if (Signal) Signal->Trigger(Obj, Data);
	}

	virtual void RegisterFuncHook() {}

	virtual UFINMultiFunctionHook* Self() { return nullptr; }

public:		
	void Register(UObject* sender) override {
		Super::Register(sender);
		
		FScopeLock Lock(&Self()->Mutex);
    	Self()->Senders.Add(Sender = sender);

		if (!Self()->bIsRegistered) {
			Self()->bIsRegistered = true;
			Self()->RegisterFuncHook();
		}
    }
		
	void Unregister() override {
		FScopeLock Lock(&Self()->Mutex);
    	Self()->Senders.Remove(Sender);
    }
};

UCLASS()
class UFINBuildableHook : public UFINStaticReflectionHook {
	GENERATED_BODY()
private:
	FDelegateHandle Handle;
	
public:
	UFUNCTION()
	void ProductionStateChanged(EProductionStatus status) {
		Send({(int64)status});
	}

	void Register(UObject* sender) override {
		Super::Register(sender);

		UFINClass* Class = FFINReflection::Get()->FindClass(Sender->GetClass());
		Signal = Class->FindFINSignal(TEXT("ProductionChanged"));

		Handle = Cast<AFGBuildable>(sender)->mOnProductionStatusChanged.AddUObject(this, &UFINBuildableHook::ProductionStateChanged);
	}

	void Unregister() override {
		Cast<AFGBuildable>(Sender)->mOnProductionStatusChanged.Remove(Handle);
	}
};

UCLASS()
class UFINRailroadTrackHook : public UFINFunctionHook {
	GENERATED_BODY()
private:
	UPROPERTY()
	UFINSignal* VehicleEnterSignal;

	UPROPERTY()
	UFINSignal* VehicleExitSignal;
	
protected:
	static UFINRailroadTrackHook* StaticSelf() {
		static UFINRailroadTrackHook* Hook = nullptr;
		if (!Hook) Hook = const_cast<UFINRailroadTrackHook*>(GetDefault<UFINRailroadTrackHook>());
		return Hook; 
	}

	// Begin UFINFunctionHook
	virtual UFINFunctionHook* Self() override {
		return StaticSelf();
	}
	// End UFINFunctionHook
	
private:
	static void VehicleEnter(AFGBuildableRailroadTrack* Track, AFGRailroadVehicle* Vehicle) {
		StaticSelf()->Send(Track, TEXT("VehicleEnter"), {(FINTrace)Vehicle});
	}

	static void VehicleExit(AFGBuildableRailroadTrack* Track, AFGRailroadVehicle* Vehicle) {
		StaticSelf()->Send(Track, TEXT("VehicleExit"), {(FINTrace)Vehicle});
	}
	
public:
	void RegisterFuncHook() override {
		SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildableRailroadTrack::OnVehicleEntered, (void*)GetDefault<AFGBuildableRailroadTrack>(), &VehicleEnter);
		SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildableRailroadTrack::OnVehicleExited, (void*)GetDefault<AFGBuildableRailroadTrack>(), &VehicleExit);
	}
};

UCLASS()
class UFINTrainHook : public UFINStaticReflectionHook {
	GENERATED_BODY()
			
public:
	UFUNCTION()
	void SelfDrvingUpdate(bool enabled) {
		Send({enabled});
	}
			
	void Register(UObject* sender) override {
		Super::Register(sender);

		UFINClass* Class = FFINReflection::Get()->FindClass(Sender->GetClass());
		Signal = Class->FindFINSignal(TEXT("SelfDrvingUpdate"));
		
		Cast<AFGTrain>(sender)->mOnSelfDrivingChanged.AddDynamic(this, &UFINTrainHook::SelfDrvingUpdate);
	}
		
	void Unregister() override {
		Cast<AFGTrain>(Sender)->mOnSelfDrivingChanged.RemoveDynamic(this, &UFINTrainHook::SelfDrvingUpdate);
	}
};

UCLASS()
class UFINRailroadStationHook : public UFINFunctionHook {
	GENERATED_BODY()
private:
	UPROPERTY()
	UFINSignal* VehicleEnterSignal;

	UPROPERTY()
	UFINSignal* VehicleExitSignal;
	
protected:
	static UFINRailroadStationHook* StaticSelf() {
		static UFINRailroadStationHook* Hook = nullptr;
		if (!Hook) Hook = const_cast<UFINRailroadStationHook*>(GetDefault<UFINRailroadStationHook>());
		return Hook; 
	}

	// Begin UFINFunctionHook
	virtual UFINFunctionHook* Self() override {
		return StaticSelf();
	}
	// End UFINFunctionHook
	
private:
	static void StartDocking(bool RetVal, AFGBuildableRailroadStation* Self, AFGLocomotive* Locomotive, float Offset) {
		StaticSelf()->Send(Self, TEXT("StartDocking"), {RetVal, (FINTrace)Locomotive, Offset});
	}
	
	static void FinishDocking(AFGBuildableRailroadStation* Self) {
		StaticSelf()->Send(Self, TEXT("FinishDocking"), {});
	}

	static void CancelDocking(AFGBuildableRailroadStation* Self) {
		StaticSelf()->Send(Self, TEXT("CancelDocking"), {});
	}
	
public:
	void RegisterFuncHook() override {
		SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildableRailroadStation::StartDocking, GetDefault<AFGBuildableRailroadStation>(), &UFINRailroadStationHook::StartDocking);
		SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildableRailroadStation::FinishDockingSequence, GetDefault<AFGBuildableRailroadStation>(), &UFINRailroadStationHook::FinishDocking);
		SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildableRailroadStation::CancelDockingSequence, GetDefault<AFGBuildableRailroadStation>(), &UFINRailroadStationHook::CancelDocking);
	}
};

UCLASS()
class UFINRailroadSignalHook : public UFINStaticReflectionHook {
	GENERATED_BODY()

	UPROPERTY()
	UFINSignal* ValidationChangedSignal;
			
public:
	UFUNCTION()
	void AspectChanged(ERailroadSignalAspect Aspect) {
		Send({(int64)Aspect});
	}

	UFUNCTION()
	void ValidationChanged(ERailroadBlockValidation Validation) {
		ValidationChangedSignal->Trigger(Sender, {(int64)Validation});
	}
	
	void Register(UObject* sender) override {
		Super::Register(sender);
		
		UFINClass* Class = FFINReflection::Get()->FindClass(Sender->GetClass());
		Signal = Class->FindFINSignal(TEXT("AspectChanged"));

		ValidationChangedSignal = Class->FindFINSignal(TEXT("ValidationChanged"));
		
		Cast<AFGBuildableRailroadSignal>(sender)->mOnAspectChangedDelegate.AddDynamic(this, &UFINRailroadSignalHook::AspectChanged);
		Cast<AFGBuildableRailroadSignal>(sender)->mOnBlockValidationChangedDelegate.AddDynamic(this, &UFINRailroadSignalHook::ValidationChanged);
	}
		
	void Unregister() override {
		Cast<AFGBuildableRailroadSignal>(Sender)->mOnAspectChangedDelegate.RemoveDynamic(this, &UFINRailroadSignalHook::AspectChanged);
		Cast<AFGBuildableRailroadSignal>(Sender)->mOnBlockValidationChangedDelegate.RemoveDynamic(this, &UFINRailroadSignalHook::ValidationChanged);
	}
};

UCLASS()
class UFINPipeHyperStartHook : public UFINMultiFunctionHook {
	GENERATED_BODY()

	protected:
	static UFINPipeHyperStartHook* StaticSelf() {
		static UFINPipeHyperStartHook* Hook = nullptr;
		if (!Hook) Hook = const_cast<UFINPipeHyperStartHook*>(GetDefault<UFINPipeHyperStartHook>());
		return Hook; 
	}
	
	// Begin UFINFunctionHook
	virtual UFINMultiFunctionHook* Self() {
		return StaticSelf();
	}
	// End UFINFunctionHook

private:

	static void EnterHyperPipe(const bool& retVal, UFGCharacterMovementComponent* CharacterMovementConstants, AFGPipeHyperStart* HyperStart) {
		if(retVal && IsValid(HyperStart)) {
			StaticSelf()->Send(HyperStart, "PlayerEntered", { FINBool(retVal)});
		}
	}
	static void ExitHyperPipe(CallScope<void(*)(UFGCharacterMovementComponent*, bool)>& call, UFGCharacterMovementComponent* charMove, bool bRagdoll){
		AActor* actor = charMove->GetTravelingPipeHyperActor();
		UObject* obj = dynamic_cast<UObject*>(actor);
		auto v = charMove->mPipeData.mConnectionToEjectThrough;
		if(IsValid(v)) {
			UFGPipeConnectionComponentBase* connection = v->mConnectedComponent;
			if(IsValid(connection)) {
				StaticSelf()->Send(connection->GetOwner(), "PlayerExited", {});
			}
		}
	} 
				
public:
	void RegisterFuncHook() override {
		SUBSCRIBE_METHOD_VIRTUAL_AFTER(UFGCharacterMovementComponent::EnterPipeHyper, (void*)GetDefault<UFGCharacterMovementComponent>(), &EnterHyperPipe);
		SUBSCRIBE_METHOD_VIRTUAL(UFGCharacterMovementComponent::PipeHyperForceExit, (void*)GetDefault<UFGCharacterMovementComponent>(), &ExitHyperPipe); 
    }
};

UCLASS()
class UFINFactoryConnectorHook : public UFINFunctionHook {
	GENERATED_BODY()

protected:
	static UFINFactoryConnectorHook* StaticSelf() {
		static UFINFactoryConnectorHook* Hook = nullptr;
		if (!Hook) Hook = const_cast<UFINFactoryConnectorHook*>(GetDefault<UFINFactoryConnectorHook>());
		return Hook; 
	}

	// Begin UFINFunctionHook
	virtual UFINFunctionHook* Self() {
		return StaticSelf();
	}
	// End UFINFunctionHook

private:
	static FCriticalSection MutexFactoryGrab;
	static TMap<TWeakObjectPtr<UFGFactoryConnectionComponent>, int8> FactoryGrabsRunning;
	
	static void LockFactoryGrab(UFGFactoryConnectionComponent* comp) {
		MutexFactoryGrab.Lock();
		++FactoryGrabsRunning.FindOrAdd(comp);
		MutexFactoryGrab.Unlock();
	}

	static bool UnlockFactoryGrab(UFGFactoryConnectionComponent* comp) {
		MutexFactoryGrab.Lock();
		int8* i = FactoryGrabsRunning.Find(comp);
		bool valid = false;
		if (i) {
			--*i;
			valid = (*i <= 0);
			if (valid) FactoryGrabsRunning.Remove(comp);
		}
		MutexFactoryGrab.Unlock();
		return valid;
	}

	static void DoFactoryGrab(UFGFactoryConnectionComponent* c, FInventoryItem& item) {
		StaticSelf()->Send(c, "ItemTransfer", {FINAny(FInventoryItem(item))});
	}

	static void FactoryGrabHook(CallScope<bool(*)(UFGFactoryConnectionComponent*, FInventoryItem&, float&, TSubclassOf<UFGItemDescriptor>)>& scope, UFGFactoryConnectionComponent* c, FInventoryItem& item, float& offset, TSubclassOf<UFGItemDescriptor> type) {
		if (!StaticSelf()->IsSender(c)) return;
		LockFactoryGrab(c);
		scope(c, item, offset, type);
		if (UnlockFactoryGrab(c) && scope.GetResult()) {
			DoFactoryGrab(c, item);
		}
	}

	static void FactoryGrabInternalHook(CallScope<bool(*)(UFGFactoryConnectionComponent*, FInventoryItem&, TSubclassOf<UFGItemDescriptor>)>& scope, UFGFactoryConnectionComponent* c, FInventoryItem& item, TSubclassOf< UFGItemDescriptor > type) {
		if (!StaticSelf()->IsSender(c)) return;
		LockFactoryGrab(c);
		scope(c, item, type);
		if (UnlockFactoryGrab(c) && scope.GetResult()) {
			DoFactoryGrab(c, item);
		}
	}
			
public:		
	void RegisterFuncHook() override {
		// TODO: Check if this works now
		// SUBSCRIBE_METHOD_MANUAL("?Factory_GrabOutput@UFGFactoryConnectionComponent@@QEAA_NAEAUFInventoryItem@@AEAMV?$TSubclassOf@VUFGItemDescriptor@@@@@Z", UFGFactoryConnectionComponent::Factory_GrabOutput, &FactoryGrabHook);
		SUBSCRIBE_METHOD(UFGFactoryConnectionComponent::Factory_GrabOutput, &FactoryGrabHook);
		SUBSCRIBE_METHOD(UFGFactoryConnectionComponent::Factory_Internal_GrabOutputInventory, &FactoryGrabInternalHook);
    }
};

UCLASS()
class UFINPipeConnectorHook : public UFINFunctionHook {
	GENERATED_BODY()

protected:
	static UFINPipeConnectorHook* StaticSelf() {
		static UFINPipeConnectorHook* Hook = nullptr;
		if (!Hook) Hook = const_cast<UFINPipeConnectorHook*>(GetDefault<UFINPipeConnectorHook>());
		return Hook; 
	}

	// Begin UFINFunctionHook
	virtual UFINFunctionHook* Self() {
		return StaticSelf();
	}
	// End UFINFunctionHook
			
public:		
	void RegisterFuncHook() override {
		// TODO: Check if this works now
		// SUBSCRIBE_METHOD_MANUAL("?Factory_GrabOutput@UFGFactoryConnectionComponent@@QEAA_NAEAUFInventoryItem@@AEAMV?$TSubclassOf@VUFGItemDescriptor@@@@@Z", UFGFactoryConnectionComponent::Factory_GrabOutput, &FactoryGrabHook);
    }
};

UCLASS()
class UFINPowerCircuitHook : public UFINFunctionHook {
	GENERATED_BODY()

protected:
	static UFINPowerCircuitHook* StaticSelf() {
		static UFINPowerCircuitHook* Hook = nullptr;
		if (!Hook) Hook = const_cast<UFINPowerCircuitHook*>(GetDefault<UFINPowerCircuitHook>());
		return Hook; 
	}

	// Begin UFINFunctionHook
	virtual UFINFunctionHook* Self() {
		return StaticSelf();
	}
	// End UFINFunctionHook
			
private:
	static void TickCircuitHook_Decl(UFGPowerCircuit*, float);
	static void TickCircuitHook(CallScope<void(*)(UFGPowerCircuit*, float)>& scope, UFGPowerCircuit* circuit, float dt) {
		bool oldFused = circuit->IsFuseTriggered();
		scope(circuit, dt);
		bool fused = circuit->IsFuseTriggered();
		if (oldFused != fused) try {
			FScopeLock Lock(&StaticSelf()->Mutex);
			TWeakObjectPtr<UObject>* sender = StaticSelf()->Senders.Find(circuit);
			if (sender) {
				UObject* obj = sender->Get();

				StaticSelf()->Send(obj, "PowerFuseChanged", {});
			}
		} catch (...) {}
	}
			
public:
	void RegisterFuncHook() override {
		// TODO: Check if this works now
		//SUBSCRIBE_METHOD_MANUAL("?TickCircuit@UFGPowerCircuit@@MEAAXM@Z", TickCircuitHook_Decl, &TickCircuitHook);
		//SUBSCRIBE_METHOD(UFGPowerCircuit::TickCircuit, &TickCircuitHook);
    }
};

UCLASS()
class UFINDroneStationHook : public UFINStaticReflectionHook {
	GENERATED_BODY()

protected:

private:

public:
};