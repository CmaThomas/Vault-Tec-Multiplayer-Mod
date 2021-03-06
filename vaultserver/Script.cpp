#include "Script.h"

vector<Script*> Script::scripts;

Script::Script( char* path )
{
	FILE* file = fopen( path, "rb" );

	if ( file == NULL )
		throw VaultException( "Script not found: %s", path );

	fclose( file );

	if ( strstr( path, ".dll" ) || strstr( path, ".so" ) )
	{
		void* handle = NULL;
#ifdef __WIN32__
		handle = LoadLibrary( path );
#else
		handle = dlopen( path, RTLD_LAZY );
#endif

		if ( handle == NULL )
			throw VaultException( "Was not able to load C++ script: %s", path );

		this->handle = handle;
		this->cpp_script = true;
		scripts.push_back( this );

		GetScriptCallback( fexec, "exec", exec );
		GetScriptCallback( fOnClientAuthenticate, "OnClientAuthenticate", _OnClientAuthenticate );
		GetScriptCallback( fOnPlayerDisconnect, "OnPlayerDisconnect", _OnPlayerDisconnect );
		GetScriptCallback( fOnPlayerRequestGame, "OnPlayerRequestGame", _OnPlayerRequestGame );
		GetScriptCallback( fOnSpawn, "OnSpawn", _OnSpawn );
		GetScriptCallback( fOnCellChange, "OnCellChange", _OnCellChange );
		GetScriptCallback( fOnActorValueChange, "OnActorValueChange", _OnActorValueChange );
		GetScriptCallback( fOnActorBaseValueChange, "OnActorBaseValueChange", _OnActorBaseValueChange );
		GetScriptCallback( fOnActorAlert, "OnActorAlert", _OnActorAlert );
		GetScriptCallback( fOnActorSneak, "OnActorSneak", _OnActorSneak );
		GetScriptCallback( fOnActorDeath, "OnActorDeath", _OnActorDeath );

		SetScriptFunction( "timestamp", &Utils::timestamp );
		SetScriptFunction( "CreateTimer", &Script::CreateTimer );
		SetScriptFunction( "CreateTimerEx", &Script::CreateTimerEx );
		SetScriptFunction( "KillTimer", &Script::KillTimer );
		SetScriptFunction( "MakePublic", &Script::MakePublic );
		SetScriptFunction( "CallPublic", &Script::CallPublic );

		SetScriptFunction( "SetServerName", &Dedicated::SetServerName );
		SetScriptFunction( "SetServerMap", &Dedicated::SetServerMap );
		SetScriptFunction( "SetServerRule", &Dedicated::SetServerRule );
		SetScriptFunction( "GetGameCode", &Dedicated::GetGameCode );

		SetScriptFunction( "ValueToString", &API::RetrieveValue_Reverse );
		SetScriptFunction( "AxisToString", &API::RetrieveAxis_Reverse );
		SetScriptFunction( "AnimToString", &API::RetrieveAnim_Reverse );

		SetScriptFunction( "GetReference", &Script::GetReference );
		SetScriptFunction( "GetBase", &Script::GetBase );
		SetScriptFunction( "GetName", &Script::GetName );
		SetScriptFunction( "GetPos", &Script::GetPos );
		SetScriptFunction( "GetAngle", &Script::GetAngle );
		SetScriptFunction( "GetCell", &Script::GetCell );

		SetScriptFunction( "GetActorValue", &Script::GetActorValue );
		SetScriptFunction( "GetActorBaseValue", &Script::GetActorBaseValue );
		SetScriptFunction( "GetActorMovingAnimation", &Script::GetActorMovingAnimation );
		SetScriptFunction( "GetActorAlerted", &Script::GetActorAlerted );
		SetScriptFunction( "GetActorSneaking", &Script::GetActorSneaking );
		SetScriptFunction( "GetActorDead", &Script::GetActorDead );
		SetScriptFunction( "IsActorJumping", &Script::IsActorJumping );

		exec();
	}

	else if ( strstr( path, ".amx" ) )
	{
		AMX* vaultscript = new AMX();

		this->handle = ( void* ) vaultscript;
		this->cpp_script = false;
		scripts.push_back( this );

		cell ret = 0;
		int err = 0;

		err = PAWN::LoadProgram( vaultscript, path, NULL );

		if ( err != AMX_ERR_NONE )
			throw VaultException( "PAWN script %s error (%d): \"%s\"", path, err, aux_StrError( err ) );

		PAWN::CoreInit( vaultscript );
		PAWN::ConsoleInit( vaultscript );
		PAWN::FloatInit( vaultscript );
		PAWN::StringInit( vaultscript );
		PAWN::FileInit( vaultscript );
		PAWN::TimeInit( vaultscript );

		err = PAWN::RegisterVaultmpFunctions( vaultscript );

		if ( err != AMX_ERR_NONE )
			throw VaultException( "PAWN script %s error (%d): \"%s\"", path, err, aux_StrError( err ) );

		err = PAWN::Exec( vaultscript, &ret, AMX_EXEC_MAIN );

		if ( err != AMX_ERR_NONE )
			throw VaultException( "PAWN script %s error (%d): \"%s\"", path, err, aux_StrError( err ) );
	}

	else
		throw VaultException( "Script type not recognized: %s", path );
}

Script::~Script()
{
	if ( this->cpp_script )
	{
#ifdef __WIN32__
		FreeLibrary( ( HINSTANCE ) this->handle );
#else
		dlclose( this->handle );
#endif
	}

	else
	{
		AMX* vaultscript = ( AMX* ) this->handle;
		PAWN::FreeProgram( vaultscript );
		delete vaultscript;
	}
}

void Script::LoadScripts( char* scripts, char* base )
{
	char* token = strtok( scripts, "," );

	try
	{
		while ( token != NULL )
		{
#ifdef __WIN32__
			Script* script = new Script( token );
#else
			char path[MAX_PATH];
			snprintf( path, sizeof( path ), "%s/%s", base, token );
			Script* script = new Script( path );
#endif
			token = strtok( NULL, "," );
		}
	}

	catch ( ... )
	{
		UnloadScripts();
		throw;
	}
}

void Script::UnloadScripts()
{
	vector<Script*>::iterator it;

	for ( it = scripts.begin(); it != scripts.end(); ++it )
		delete *it;

	Timer::TerminateAll();
	Public::DeleteAll();
	scripts.clear();
}

void Script::GetArguments( vector<boost::any>& params, va_list args, string def )
{
	string::iterator it;
	params.reserve( def.length() );

	try
	{
		for ( it = def.begin(); it != def.end(); ++it )
		{
			switch ( *it )
			{
				case 'i':
					{
						params.push_back( va_arg( args, unsigned int ) );
						break;
					}

				case 'l':
					{
						params.push_back( va_arg( args, unsigned long long ) );
						break;
					}

				case 'f':
					{
						params.push_back( va_arg( args, double ) );
						break;
					}

				case 's':
					{
						params.push_back( string( va_arg( args, const char* ) ) );
						break;
					}

				default:
					throw VaultException( "C++ call: Unknown argument identifier %02X", *it );
			}
		}
	}

	catch ( ... )
	{
		va_end( args );
		throw;
	}
}

NetworkID Script::CreateTimer( ScriptFunc timer, unsigned int interval )
{
	Timer* t = new Timer( timer, string(), vector<boost::any>(), interval );
	return t->GetNetworkID();
}

NetworkID Script::CreateTimerEx( ScriptFunc timer, unsigned int interval, string def, ... )
{
	vector<boost::any> params;

	va_list args;
	va_start( args, def );
	GetArguments( params, args, def );
	va_end( args );

	Timer* t = new Timer( timer, def, params, interval );
	return t->GetNetworkID();
}

NetworkID Script::CreateTimerPAWN( ScriptFuncPAWN timer, AMX* amx, unsigned int interval )
{
	Timer* t = new Timer( timer, amx, string(), vector<boost::any>(), interval );
	return t->GetNetworkID();
}

NetworkID Script::CreateTimerPAWNEx( ScriptFuncPAWN timer, AMX* amx, unsigned int interval, string def, const vector<boost::any>& args )
{
	Timer* t = new Timer( timer, amx, def, args, interval );
	return t->GetNetworkID();
}

void Script::KillTimer( NetworkID id )
{
	Timer::Terminate( id );
}

void Script::MakePublic( ScriptFunc _public, string name, string def )
{
	new Public( _public, name, def );
}

void Script::MakePublicPAWN( ScriptFuncPAWN _public, AMX* amx, string name, string def )
{
	new Public( _public, amx, name, def );
}

unsigned long long Script::CallPublic( string name, ... )
{
	vector<boost::any> params;
	string def = Public::GetDefinition( name );

	va_list args;
	va_start( args, name );
	GetArguments( params, args, def );
	va_end( args );

	return Public::Call( name, params );
}

unsigned long long Script::CallPublicPAWN( string name, const vector<boost::any>& args )
{
	return Public::Call( name, args );
}

bool Script::OnClientAuthenticate( string name, string pwd )
{
	vector<Script*>::iterator it;
	bool result;

	for ( it = scripts.begin(); it != scripts.end(); ++it )
	{
		if ( ( *it )->cpp_script )
			result = ( *it )->_OnClientAuthenticate( name, pwd );

		else
			result = ( bool ) PAWN::Call( ( AMX* ) ( *it )->handle, "OnClientAuthenticate", "ss", 0, pwd.c_str(), name.c_str() );
	}

	return result;
}

unsigned int Script::OnPlayerRequestGame( FactoryObject reference )
{
	vector<Script*>::iterator it;
	NetworkID id = ( *reference )->GetNetworkID();
	unsigned int result;

	for ( it = scripts.begin(); it != scripts.end(); ++it )
	{
		if ( ( *it )->cpp_script )
			result = ( *it )->_OnPlayerRequestGame( id );

		else
			result = ( unsigned int ) PAWN::Call( ( AMX* ) ( *it )->handle, "OnPlayerRequestGame", "l", 0, id );
	}

	return result;
}

void Script::OnPlayerDisconnect( FactoryObject reference, unsigned char reason )
{
	vector<Script*>::iterator it;
	NetworkID id = ( *reference )->GetNetworkID();

	for ( it = scripts.begin(); it != scripts.end(); ++it )
	{
		if ( ( *it )->cpp_script )
			( *it )->_OnPlayerDisconnect( id, reason );

		else
			PAWN::Call( ( AMX* ) ( *it )->handle, "OnPlayerDisconnect", "il", 0, ( unsigned int ) reason, id );
	}
}

void Script::OnCellChange( FactoryObject reference, unsigned int cell )
{
	vector<Script*>::iterator it;
	NetworkID id = ( *reference )->GetNetworkID();

	for ( it = scripts.begin(); it != scripts.end(); ++it )
	{
		if ( ( *it )->cpp_script )
			( *it )->_OnCellChange( id, cell );

		else
			PAWN::Call( ( AMX* ) ( *it )->handle, "OnCellChange", "il", 0, cell, id );
	}
}

void Script::OnActorValueChange( FactoryObject reference, unsigned char index, bool base, double value )
{
	vector<Script*>::iterator it;
	NetworkID id = ( *reference )->GetNetworkID();

	for ( it = scripts.begin(); it != scripts.end(); ++it )
	{
		if ( ( *it )->cpp_script )
		{
			if ( base )
				( *it )->_OnActorBaseValueChange( id, index, value );

			else
				( *it )->_OnActorValueChange( id, index, value );
		}

		else
		{
			if ( base )
				PAWN::Call( ( AMX* ) ( *it )->handle, "OnActorBaseValueChange", "fil", 0, value, ( unsigned int ) index, id );

			else
				PAWN::Call( ( AMX* ) ( *it )->handle, "OnActorValueChange", "fil", 0, value, ( unsigned int ) index, id );
		}
	}
}

void Script::OnActorAlert( FactoryObject reference, bool alerted )
{
	vector<Script*>::iterator it;
	NetworkID id = ( *reference )->GetNetworkID();

	for ( it = scripts.begin(); it != scripts.end(); ++it )
	{
		if ( ( *it )->cpp_script )
			( *it )->_OnActorAlert( id, alerted );

		else
			PAWN::Call( ( AMX* ) ( *it )->handle, "OnActorAlert", "il", 0, ( unsigned int ) alerted, id );
	}
}

void Script::OnActorSneak( FactoryObject reference, bool sneaking )
{
	vector<Script*>::iterator it;
	NetworkID id = ( *reference )->GetNetworkID();

	for ( it = scripts.begin(); it != scripts.end(); ++it )
	{
		if ( ( *it )->cpp_script )
			( *it )->_OnActorSneak( id, sneaking );

		else
			PAWN::Call( ( AMX* ) ( *it )->handle, "OnActorSneak", "il", 0, ( unsigned int ) sneaking, id );
	}
}

unsigned int Script::GetReference( NetworkID id )
{
	unsigned int value = 0;

	FactoryObject reference = GameFactory::GetObject( id );
	Object* object = vaultcast<Object>( reference );

	if ( object )
		value = object->GetReference();

	return value;
}

unsigned int Script::GetBase( NetworkID id )
{
	unsigned int value = 0;

	FactoryObject reference = GameFactory::GetObject( id );
	Object* object = vaultcast<Object>( reference );

	if ( object )
		value = object->GetBase();

	return value;
}

string Script::GetName( NetworkID id )
{
	string name = "";

	FactoryObject reference = GameFactory::GetObject( id );
	Object* object = vaultcast<Object>( reference );

	if ( object )
		name = object->GetName();

	return name;
}

void Script::GetPos( NetworkID id, double& X, double& Y, double& Z )
{
	X = 0.00;
	Y = 0.00;
	Z = 0.00;

	FactoryObject reference = GameFactory::GetObject( id );
	Object* object = vaultcast<Object>( reference );

	if ( object )
	{
		X = object->GetGamePos( Axis_X );
		Y = object->GetGamePos( Axis_Y );
		Z = object->GetGamePos( Axis_Z );
	}
}

void Script::GetAngle( NetworkID id, double& X, double& Y, double& Z )
{
	X = 0.00;
	Y = 0.00;
	Z = 0.00;

	FactoryObject reference = GameFactory::GetObject( id );
	Object* object = vaultcast<Object>( reference );

	if ( object )
	{
		X = object->GetAngle( Axis_X );
		Y = object->GetAngle( Axis_Y );
		Z = object->GetAngle( Axis_Z );
	}
}

unsigned int Script::GetCell( NetworkID id )
{
	unsigned int value = 0;

	FactoryObject reference = GameFactory::GetObject( id );
	Object* object = vaultcast<Object>( reference );

	if ( object )
		value = object->GetGameCell();

	return value;
}

double Script::GetActorValue( NetworkID id, unsigned char index )
{
	double value = 0.00;

	FactoryObject reference = GameFactory::GetObject( id );
	Actor* actor = vaultcast<Actor>( reference );

	if ( actor )
		value = actor->GetActorValue( index );

	return value;
}

double Script::GetActorBaseValue( NetworkID id, unsigned char index )
{
	double value = 0.00;

	FactoryObject reference = GameFactory::GetObject( id );
	Actor* actor = vaultcast<Actor>( reference );

	if ( actor )
		value = actor->GetActorBaseValue( index );

	return value;
}

unsigned char Script::GetActorMovingAnimation( NetworkID id )
{
	unsigned char index = 0x00;

	FactoryObject reference = GameFactory::GetObject( id );
	Actor* actor = vaultcast<Actor>( reference );

	if ( actor )
		index = actor->GetActorMovingAnimation();

	return index;
}

bool Script::GetActorAlerted( NetworkID id )
{
	bool state = false;

	FactoryObject reference = GameFactory::GetObject( id );
	Actor* actor = vaultcast<Actor>( reference );

	if ( actor )
		state = actor->GetActorAlerted();

	return state;
}

bool Script::GetActorSneaking( NetworkID id )
{
	bool state = false;

	FactoryObject reference = GameFactory::GetObject( id );
	Actor* actor = vaultcast<Actor>( reference );

	if ( actor )
		state = actor->GetActorSneaking();

	return state;
}

bool Script::GetActorDead( NetworkID id )
{
	bool state = false;

	FactoryObject reference = GameFactory::GetObject( id );
	Actor* actor = vaultcast<Actor>( reference );

	if ( actor )
		state = actor->GetActorDead();

	return state;
}

bool Script::IsActorJumping( NetworkID id )
{
	bool state = false;

	FactoryObject reference = GameFactory::GetObject( id );
	Actor* actor = vaultcast<Actor>( reference );

	if ( actor )
		state = actor->IsActorJumping();

	return state;
}
