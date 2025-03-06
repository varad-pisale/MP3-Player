#include <iostream>    // For input and output (cout, cin)
#include <filesystem>  // For file system operations (list directories, check files)
#include <string>      // For string manipulation (std::string)
#include <vector>      // For dynamic arrays (std::vector)
#include <windows.h>   // For Windows API functions (console manipulation, Sleep)
#include <mfapi.h>     // For initializing Media Foundation
#include <mfplay.h>    // For media playback (IMFPMediaPlayer)
#include <mfidl.h>     // For media object interfaces (IMFMediaSource)
#include <mfobjects.h> // For Media Foundation object types (IMFAttributes)
#include <cstdlib>     // For system commands (system(), exit())
#include <thread>      // Required for std::this_thread::sleep_for
#include <ctime>       // For srand()
#include <set>         // For set<int>

#pragma comment(lib, "Mfplat.lib") // Links Media Foundation platform library (initialization, attributes)
#pragma comment(lib, "Mfplay.lib") // Links Media Foundation playback library (IMFPMediaPlayer)
#pragma comment(lib, "Mf.lib")     // Links core Media Foundation library (media session, sources)
#pragma comment(lib, "User32.lib") // Links Windows User32 library (message boxes, window management)

using namespace std;
namespace fs = filesystem;

// Custom event handler
class MediaPlayerCallback : public IMFPMediaPlayerCallback
{
public:
    HANDLE hEvent; // Event to signal when playback ends

    MediaPlayerCallback() { hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr); }

    virtual void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER *pEventHeader)
    {
        if (pEventHeader->eEventType == MFP_EVENT_TYPE_PLAYBACK_ENDED)
        {
            SetEvent(hEvent); // Signal that playback has ended
        }
    }

    STDMETHOD_(ULONG, AddRef)() { return 1; }
    STDMETHOD_(ULONG, Release)() { return 1; }
    STDMETHOD(QueryInterface)(REFIID, void **) { return E_NOINTERFACE; }
};

class song
{
    atomic<bool> playing; // Thread-safe flag
public:
    song() : playing(false) {}

    void play_Song(const string &song_Name)
    {
        MediaPlayerCallback *callback = new MediaPlayerCallback(); // Create callback instance
        // Convert UTF-8 string to UTF-16 wstring
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, song_Name.c_str(), -1, nullptr, 0);
        wstring wideSongName(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, song_Name.c_str(), -1, &wideSongName[0], size_needed);

        if (!fs::exists(song_Name))
        {
            cout << "File not found: " << song_Name << endl;
            return;
        }

        IMFPMediaPlayer *pPlayer = nullptr;
        MFStartup(MF_VERSION);

        HRESULT hr = MFPCreateMediaPlayer(wideSongName.c_str(), FALSE, 0, callback, nullptr, &pPlayer);

        if (SUCCEEDED(hr))
        {
            playing = true; // Mark as playing
            pPlayer->Play();
            pPlayer->AddRef(); // Keep the player alive
            cout << "Playing :" << song_Name;

            while (WaitForSingleObject(callback->hEvent, 100) == WAIT_TIMEOUT)
            {
                MSG msg;
                while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }

            playing = false; // Mark as stopped
            cout << endl
                 << "Song finished." << endl;
            pPlayer->Shutdown();
            pPlayer->Release();

            return;
        }
        else
        {
            cout << "Error playing song! HRESULT: " << hr << endl;
        }

        MFShutdown();
    }

    bool isPlaying()
    {
        return playing;
    }
};

class startup_Menu
{

    string playlist_name, song_Name;
    vector<string> file_Names; // Store file names

public:
    // function to list folders
    string list_Folders(int *folder_selected, int *index)
    {
        vector<string> folder_Name;             // to store all folder name
        fs::path exe_Path = fs::current_path(); // to get path of cuurent directory
        cout << endl
             << "Available Playlists are :" << endl;
        for (auto const &folders : fs::directory_iterator(exe_Path)) // the iterator iterates over all folder
        {
            if (folders.is_directory()) // checks if its a folder and not a file
            {
                cout << *index << ". " << folders.path().filename() << endl;
                folder_Name.push_back(folders.path().filename().string()); // adds the name to the folder list
                (*index)++;
            }
        }
        cout << "Enter serial to open that folder or 0 to Exit:" << endl
             << "-> " << endl;
        cin >> *folder_selected;
        cin.ignore();

        if (*folder_selected > 0 && *folder_selected <= folder_Name.size())
        {
            return folder_Name[*folder_selected - 1];
        }
        else
        {
            return ""; // Return an empty string if the selection is invalid or 0
        }
    }

    // Function to list files in a folder
    vector<string> list_Files(const string &folder_Path)
    {
        for (auto const &entry : fs::directory_iterator(folder_Path)) // Iterate through folder
        {
            if (entry.is_regular_file()) // Check if it's a file
            {
                file_Names.push_back(entry.path().string()); // Store file name with path
            }
        }

        return file_Names; // Return list of file names
    }

    void select_Song()
    {
        int song_Number, index = 1;
        vector<string> songs;                                             // Store song names for selection
        for (auto const &folders : fs::directory_iterator(playlist_name)) // the iterator iterates over all folder
        {
            if (folders.is_regular_file()) // checks if its a file
            {
                cout << index << ". " << folders.path().filename() << endl;
                songs.push_back(folders.path().string()); // Store full path
                index++;
            }
        }
        // non english filenames will cause error so rename in english

        cout << "Enter serial to play the song or 0 to Exit:" << endl
             << "-> ";
        cin >> song_Number;

        if (song_Number == 0)
        {
            return;
        }

        if (song_Number > index || song_Number <= 0)
        {
            cout << "Input valid serial number.";
            select_Song();
        }
        else
        {
            song_Name = songs[song_Number - 1];
            song s;
            s.play_Song(song_Name);
        }
    }

    void open_Playlist()
    {
        int select_Folder, index = 1;
        string folder_NAME;

        folder_NAME = list_Folders(&select_Folder, &index);
        if (select_Folder == 0)
        {
            return;
        }

        if (select_Folder > index - 1 || select_Folder <= 0)
        {
            cout << "Input valid serial number.";
            open_Playlist();
        }
        else
        {
            playlist_name = folder_NAME;
            select_Song();
        }
    }

    void delete_Playlist()
    {
        int select_Folder, index = 1;
        string folder_Path;

        folder_Path = list_Folders(&select_Folder, &index);
        if (fs::exists(folder_Path))
        {
            fs::remove_all(folder_Path); // Deletes folder and all its contents
            std::cout << "Folder deleted successfully.\n";
        }
        else
        {
            std::cout << "Folder does not exist.\n";
        }
    }

    void opt_Playlist()
    {
        int option, select_Folder, index = 1;
        string loop_Folder;

        loop_Folder = list_Folders(&select_Folder, &index);

        if (select_Folder == 0)
        {
            return;
        }

        if (select_Folder > index - 1 || select_Folder <= 0)
        {
            cout << "Input valid serial number.";
            open_Playlist();
        }
        else
        {
            cout << "\n1. Loop Playlist";
            cout << "\n2. Shuffle Playlist" << endl
                 << "-> ";
            cin >> option;
            if (option == 1)
            {
                loop_Playlist(loop_Folder);
            }
            else if (option == 2)
            {
                shuffle_Playlist(loop_Folder);
            }
            else
            {
                opt_Playlist();
            }
        }
    }

    void loop_Playlist(const string &folder)
    {
        system("cls");
        song ply;
        vector<string> songs_List;
        songs_List = list_Files(folder);
        for (int i = 0; i < songs_List.size(); i++)
        {
            ply.play_Song(songs_List[i]);

            // Wait until the song finishes
            while (ply.isPlaying())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }

    void shuffle_Playlist(const string &folder)
    {
        system("cls");
        set<int> used_Numbers;
        song ply;
        vector<string> songs_List = list_Files(folder);
        int n = songs_List.size(); // Get number of songs

        for (int i = 0; i < n; i++)
        {
            int randomIndex;
            do
            {
                randomIndex = rand() % n; // Generate number between 0 and n-1
            } while (used_Numbers.find(randomIndex) != used_Numbers.end()); // Check if already used

            used_Numbers.insert(randomIndex); // Store used number
            ply.play_Song(songs_List[randomIndex]);

            // Wait until the song finishes
            while (ply.isPlaying())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }

    void close_App()
    {
        cout << "\t\tGOOD BYE!!!" << endl<<"\tEnter any key to EXIT.";
        getchar();
        getchar();
        return;
    }

    void open_Screen()
    {
        bool loop = true;
        while (loop)
        {
            int option;
            system("cls");

            cout << endl;
            cout << "-----WELCOME TO MP3 PLAYER-----" << endl;
            cout << "\tWhat would you like to do? " << endl;
            cout << "\t\t1. Open Playlist" << endl;
            cout << "\t\t2. Delete Playlist" << endl;
            cout << "\t\t3. Loop/Shuffle the Playlist" << endl;
            cout << "\t\t4. Exit" << endl;
            cout << "\t\t\t-> ";
            cin >> option;

            switch (option)
            {
            case 1:
                open_Playlist();
                break;
            case 2:
                delete_Playlist();
                break;
            case 3:
                opt_Playlist();
                break;
            case 4:
                close_App();
                loop = false;
                break;

            default:
                break;
            }
        }
    }
    friend class song;
};

int main()
{
    startup_Menu menu;
    menu.open_Screen();

    return 0;
}