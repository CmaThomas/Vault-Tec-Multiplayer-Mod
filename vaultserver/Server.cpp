#include "Server.h"

#ifdef VAULTMP_DEBUG
Debug* Server::debug = NULL;
#endif

#ifdef VAULTMP_DEBUG
void Server::SetDebugHandler( Debug* debug )
{
	Server::debug = debug;

	if ( debug != NULL )
		debug->Print( "Attached debug handler to Server class", true );
}
#endif

NetworkResponse Server::Authenticate( RakNetGUID guid, string name, string pwd )
{
	NetworkResponse response;
	bool result = Script::OnClientAuthenticate( name, pwd );

	if ( result )
	{
		for ( ModList::iterator it = Dedicated::modfiles.begin(); it != Dedicated::modfiles.end(); ++it )
		{
			pDefault* packet = PacketFactory::CreatePacket( ID_GAME_MOD, it->first.c_str(), it->second );
			response.push_back( Network::CreateResponse( packet,
								( unsigned char ) HIGH_PRIORITY,
								( unsigned char ) RELIABLE_ORDERED,
								CHANNEL_GAME,
								guid ) );
		}

		pDefault* packet = PacketFactory::CreatePacket( ID_GAME_START, Dedicated::savegame.first.c_str(), Dedicated::savegame.second );
		response.push_back( Network::CreateResponse( packet,
							( unsigned char ) HIGH_PRIORITY,
							( unsigned char ) RELIABLE_ORDERED,
							CHANNEL_GAME,
							guid ) );
	}

	else
	{
		pDefault* packet = PacketFactory::CreatePacket( ID_GAME_END, ID_REASON_DENIED );
		response = Network::CompleteResponse( Network::CreateResponse( packet,
											  ( unsigned char ) HIGH_PRIORITY,
											  ( unsigned char ) RELIABLE_ORDERED,
											  CHANNEL_GAME,
											  guid ) );
	}

	return response;
}

NetworkResponse Server::LoadGame( RakNetGUID guid )
{
	NetworkResponse response;

	pDefault* packet = PacketFactory::CreatePacket( ID_GAME_LOAD );
	response.push_back( Network::CreateResponse( packet,
						( unsigned char ) HIGH_PRIORITY,
						( unsigned char ) RELIABLE_ORDERED,
						CHANNEL_GAME,
						guid ) );

	return response;
}

NetworkResponse Server::NewPlayer( RakNetGUID guid, NetworkID id, string name )
{
	NetworkResponse response;
	GameFactory::CreateKnownInstance( ID_PLAYER, id, 0x00000000 );
	FactoryObject _player = GameFactory::GetObject( id );
	Player* player = vaultcast<Player>( _player );

	player->SetName( name );

	Client* client = new Client( guid, player->GetNetworkID() );
	Dedicated::self->SetServerPlayers( pair<int, int>( Client::GetClientCount(), Dedicated::connections ) );

	unsigned int result = Script::OnPlayerRequestGame( _player );

	if ( !result )
		throw VaultException( "Script did not provide an actor base for a player" );

	player->SetBase( result );

	pDefault* packet = PacketFactory::CreatePacket( ID_PLAYER_NEW, id, 0x00000000, player->GetBase(), name.c_str() );
	response.push_back( Network::CreateResponse( packet,
						( unsigned char ) HIGH_PRIORITY,
						( unsigned char ) RELIABLE_ORDERED,
						CHANNEL_GAME,
						Client::GetNetworkList( client ) ) );

	return response;
}

NetworkResponse Server::Disconnect( RakNetGUID guid, unsigned char reason )
{
	NetworkResponse response;
	Client* client = Client::GetClientFromGUID( guid );

	if ( client != NULL )
	{
		FactoryObject reference = GameFactory::GetObject( client->GetPlayer() );
		Script::OnPlayerDisconnect( reference, reason );
		delete client;

		NetworkID id = GameFactory::DestroyInstance( reference );

		pDefault* packet = PacketFactory::CreatePacket( ID_PLAYER_LEFT, id );
		response = Network::CompleteResponse( Network::CreateResponse( packet,
											  ( unsigned char ) HIGH_PRIORITY,
											  ( unsigned char ) RELIABLE_ORDERED,
											  CHANNEL_GAME,
											  Client::GetNetworkList( NULL ) ) );

		Dedicated::self->SetServerPlayers( pair<int, int>( Client::GetClientCount(), Dedicated::connections ) );
	}

	return response;
}

NetworkResponse Server::GetPos( RakNetGUID guid, FactoryObject reference, double X, double Y, double Z )
{
	NetworkResponse response;
	Object* object = vaultcast<Object>( reference );
	bool result = ( ( bool ) object->SetGamePos( Axis_X, X ) | ( bool ) object->SetGamePos( Axis_Y, Y ) | ( bool ) object->SetGamePos( Axis_Z, Z ) );

	if ( result )
	{
		pDefault* packet = PacketFactory::CreatePacket( ID_UPDATE_POS, object->GetNetworkID(), X, Y, Z );
		response = Network::CompleteResponse( Network::CreateResponse( packet,
											  ( unsigned char ) HIGH_PRIORITY,
											  ( unsigned char ) RELIABLE_SEQUENCED,
											  CHANNEL_GAME,
											  Client::GetNetworkList( guid ) ) );
	}

	return response;
}

NetworkResponse Server::GetAngle( RakNetGUID guid, FactoryObject reference, unsigned char axis, double value )
{
	NetworkResponse response;
	Object* object = vaultcast<Object>( reference );
	bool result = ( bool ) object->SetAngle( axis, value );

	if ( result && axis != Axis_X )
	{
		pDefault* packet = PacketFactory::CreatePacket( ID_UPDATE_ANGLE, object->GetNetworkID(), axis, value );
		response = Network::CompleteResponse( Network::CreateResponse( packet,
											  ( unsigned char ) HIGH_PRIORITY,
											  ( unsigned char ) RELIABLE_SEQUENCED,
											  CHANNEL_GAME,
											  Client::GetNetworkList( guid ) ) );
	}

	return response;
}

NetworkResponse Server::GetGameCell( RakNetGUID guid, FactoryObject reference, unsigned int cell )
{
	NetworkResponse response;
	Object* object = vaultcast<Object>( reference );
	bool result = ( bool ) object->SetGameCell( cell );

	if ( result )
	{
		pDefault* packet = PacketFactory::CreatePacket( ID_UPDATE_CELL, object->GetNetworkID(), cell );
		response = Network::CompleteResponse( Network::CreateResponse( packet,
											  ( unsigned char ) HIGH_PRIORITY,
											  ( unsigned char ) RELIABLE_SEQUENCED,
											  CHANNEL_GAME,
											  Client::GetNetworkList( guid ) ) );

		Script::OnCellChange( reference, cell );
	}

	return response;
}

NetworkResponse Server::GetActorValue( RakNetGUID guid, FactoryObject reference, bool base, unsigned char index, double value )
{
	Actor* actor = vaultcast<Actor>( reference );

	if ( !actor )
		throw VaultException( "Object with reference %08X is not an Actor", ( *reference )->GetReference() );

	NetworkResponse response;
	bool result;

	if ( base )
		result = ( bool ) actor->SetActorBaseValue( index, value );

	else
		result = ( bool ) actor->SetActorValue( index, value );

	if ( result )
	{
		pDefault* packet = PacketFactory::CreatePacket( ID_UPDATE_VALUE, actor->GetNetworkID(), base, index, value );
		response = Network::CompleteResponse( Network::CreateResponse( packet,
											  ( unsigned char ) HIGH_PRIORITY,
											  ( unsigned char ) RELIABLE_ORDERED,
											  CHANNEL_GAME,
											  Client::GetNetworkList( guid ) ) );

		Script::OnActorValueChange( reference, index, base, value );
	}

	return response;
}

NetworkResponse Server::GetActorState( RakNetGUID guid, FactoryObject reference, unsigned char index, unsigned char moving, bool alerted, bool sneaking )
{
	Actor* actor = vaultcast<Actor>( reference );

	if ( !actor )
		throw VaultException( "Object with reference %08X is not an Actor", ( *reference )->GetReference() );

	NetworkResponse response;
	bool result, _alerted, _sneaking;

	_alerted = ( bool ) actor->SetActorAlerted( alerted );
	_sneaking = ( bool ) actor->SetActorSneaking( sneaking );
	result = ( ( bool ) actor->SetActorMovingAnimation( index ) | ( bool ) actor->SetActorMovingXY( moving ) | _alerted | _sneaking );

	if ( result )
	{
		pDefault* packet = PacketFactory::CreatePacket( ID_UPDATE_STATE, actor->GetNetworkID(), index, moving, alerted, sneaking );
		response = Network::CompleteResponse( Network::CreateResponse( packet,
											  ( unsigned char ) HIGH_PRIORITY,
											  ( unsigned char ) RELIABLE_ORDERED,
											  CHANNEL_GAME,
											  Client::GetNetworkList( guid ) ) );

		if ( _alerted )
			Script::OnActorAlert( reference, alerted );

		if ( _sneaking )
			Script::OnActorSneak( reference, sneaking );
	}

	return response;
}
