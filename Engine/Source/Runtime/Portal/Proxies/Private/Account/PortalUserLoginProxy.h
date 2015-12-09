// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class IPortalService;
class IMessageRpcClient;

class FPortalUserLoginProxyFactory
{
public:
	static TSharedRef<IPortalService> Create(const TSharedRef<IMessageRpcClient>& RpcClient);
}; 