package sw3.server;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketException;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;
import java.util.Scanner;
import java.util.logging.ConsoleHandler;
import java.util.logging.Handler;
import java.util.logging.Level;
import java.util.logging.Logger;

import sw3.lib.girafweb.Profile;

/**
 * The server class is a complete server solution, that loads up according to a
 * configuration file and runs indefinitely until closed. The server can run in
 * daemon (unattended) or active (console-based) form. In active form a console
 * is made available through which to pass commands to the server itself. In
 * daemon form this single functionality is disabled, although the logging
 * output is still available.
 * 
 * @see
 * @author Johannes Lindhart Borresen
 */
public class ServerThread extends Thread
{
    // / Config file "constants".
    public final String CONFIG_DBHOST = "dbhost";
    public final String CONFIG_DBUSER = "dbuser";
    public final String CONFIG_DBPASS = "dbpass";
    public final String CONFIG_DB = "db";
    public final String CONFIG_DBPORT = "dbport";
    public final String CONFIG_SERVERMODE = "servermode";
    public final String CONFIG_SERVERPORT = "serverport";

    /**
     * Properties object that contains all the configuration data garnered from
     * the local config.ini file or default values.
     */
    public Properties config = new Properties();

    /**
     * Path to the config file.
     */
    public String configPath = "./config.ini";

    /**
     * Primary socket for incoming connections.
     */
    public ServerSocket serverSocket;
    
    /**
     * The logger used to display and control information as it pours onto the
     * terminal screen.
     */
    public Logger log;

    /**
     * Used together with log to create a command-line interface for the server.
     */
    public Scanner input;

    /**
     * Determines whether the server offers an active console or not.
     */
    private ServerModeEnum _mode;

    /**
     * Reference to the
     */
    public GirafSqlHelper girafDb;

    /**
     * Current signal for the server thread. Use this to communicate desired
     * changes without the need for methods or heavy chaining. Just change the
     * signal and wait a cycle.
     */
    protected CommandOpEnum loopSignal;

    /**
     * Separate thread that listens for, and accepts, incoming connections.
     */
    public SocketThread connectionListener;
    
    public ArrayList<ConnectionThread> connections;
    
    /**
     * Get the current running mode of the server. Can be "daemon" for
     * non-interactive mode or "active" for an active console-based mode.
     * 
     * @return The current running mode.
     */
    public ServerModeEnum getMode()
    {
        return _mode;
    }

    protected void setMode(ServerModeEnum newMode)
    {
        _mode = newMode;
    }

    public ServerThread()
    {
        try
        {
            // Important. Start by creating a logger we can see.
            _createLogger();
            // THEN get any pre-execution information from the on-file config.
            loadConfig();

            // And theeeen, do everything else.
            _createGirafDb();

            // Create the server socket.
            _createServerSocket();
            
            // Creates the connection listener.
            _createListener();
            
            // Activate the input scanner if
            if (getConf(CONFIG_SERVERMODE).compareToIgnoreCase("active") == 0)
                _createScanner();
        } catch (Exception e)
        {
            // TODO Auto-generated catch block
            log.severe("An error occurred during startup:\n" + e.getMessage()
                    + "\nThe server will now shut down");
            e.printStackTrace();
            loopSignal = CommandOpEnum.STARTUPERR;
        }
        finally
        {
            log.info("LOG: Server has started up. Be happy!");
        }
        // At this point we await the start command from our calling
        // environment,
        // at which point run is called and we start out server loop.
    }

    /**
     * Creates the socket listener thread.
     */
    private void _createListener()
    {
        connections = new ArrayList<ConnectionThread>();
        
        ServerThread.SocketThread listen = this.new SocketThread();
        connectionListener = listen;
        connectionListener.start();
        log.finer("Listener thread started.");
    }
    
    /**
     * Creates the SqlHelper specific for giraf, loads up parameters from the
     * config file and tries to connect.
     */
    private void _createGirafDb()
    {
        String host, user, pass, db;
        int port;

        host = getConf(CONFIG_DBHOST);
        user = getConf(CONFIG_DBUSER);
        pass = getConf(CONFIG_DBPASS);
        db = getConf(CONFIG_DB);
        port = Integer.parseInt(getConf(CONFIG_DBPORT));

        try
        {
            girafDb = new GirafSqlHelper(host, port, user, pass, db);
        } catch (SQLException e)
        {
            // TODO Auto-generated catch block
            // e.printStackTrace();
            log.severe("Failed to connect to the backend database:\n"
                    + e.getMessage());
            e.printStackTrace();
            shutdown();
        }
    }

    /**
     * Retrieves the configuration value of a specific key.
     * 
     * @param key
     *            The key to get a value for.
     * @return The value, or null if unspecified (we hope not):
     */
    public String getConf(String key)
    {
        return config.getProperty(key);
    }

    @Override
    public void run()
    {
        if (loopSignal == CommandOpEnum.NEWCONF)
        {
            log.warning("A new config file was created. Server shutting down so you can configure.");
            loopSignal = CommandOpEnum.TERM;
        } else if (loopSignal == CommandOpEnum.STARTUPERR)
        {
            log.severe("Due to a critical startup error, the server will now shut down.");
            loopSignal = CommandOpEnum.TERM;
        }

        // We loop indefinitely until the loopSignal field has been set
        // to TERM. For now, only NOOP and TERM are supported.
        while (loopSignal != CommandOpEnum.TERM)
        {
            // We only take user input if we're running active mode.
            if (getMode() == ServerModeEnum.ACTIVE)
                runScanner();
        }
        // If we break loop, we do a clean shutdown.
        shutdown();
    }

    /**
     * Runs the scanner section of a loop. The scanner reads console input and
     * reacts to it, if applicable. May change loopSignal if applicable.
     */
    protected void runScanner()
    {
        // If the scanner has registered some user input, we start reacting to
        // it.
        if (input.hasNext())
        {
            String cmd;
            // Currently, that entails printing it like an idiot.
            cmd = input.next();
            log.info("Command: " + cmd);

            // If the command is quit, however, we return the term signal.
            if (cmd.compareToIgnoreCase("quit") == 0)
                loopSignal = CommandOpEnum.TERM;
            else if (cmd.compareToIgnoreCase("profiles") == 0)
            {
                List<Profile> profiles = girafDb.getProfiles();
                
                log.info("Following profiles are registered in the database.");
                
                for (Profile p : profiles)
                {
                    log.info(String.format("(%s): %s, %s", p.getId(), p.getName(), p.getAge()));      
                }
            }

            System.out.print(">");
        }
    }

    public void shutdown()
    {
        try
        {
            log.info("Shutting down the server thread.");
            connectionListener.close();
            if (serverSocket != null)
                serverSocket.close();
        }
        catch (Exception e)
        {
            // TODO Auto-generated catch block
            // log.warning("The socket could not be closed:\n" +
            log.warning(e.getMessage());
            e.printStackTrace();
        }
    }

    private void _createScanner()
    {
        input = new Scanner(System.in);
    }

    private void _createServerSocket()
    {
        try
        {
            serverSocket = new ServerSocket(Integer.parseInt(getConf(CONFIG_SERVERPORT)));
            serverSocket.setSoTimeout(500);
        } catch (NumberFormatException e)
        {
            // TODO Auto-generated catch block
            // e.printStackTrace();
            log.severe("Failed to load the port setting from config.ini:\n"
                    + e.getMessage());
        } catch (IOException e)
        {
            // TODO Auto-generated catch block
            log.severe("Failed to load the port setting from config.ini:\n"
                    + e.getMessage());
            e.printStackTrace();
        }
    }

    public void loadConfig()
    {
        if (!configExists())
        {
            log.info("Creating new config file.");
            createConfig();
        }

        try
        {
            config.load(new FileInputStream(getConfigFile()));
            // log.info("Configuration file loaded successfully from path:\n"
                    // + getConfigFile().getAbsolutePath());
        }
        catch (FileNotFoundException e)
        {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        catch (IOException e)
        {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }

        // Now we have the config file prepped. Let's get important data.
        setMode(ServerModeEnum.ACTIVE);
    }

    /**
     * Retrieves a File object with a path leading to the config file prope.
     * Be aware that the file may not exist.
     * @return Handler to the config file.
     */
    public File getConfigFile()
    {
        return new File(configPath);
    }

    /**
     * Creates a new configuration file with default values.
     * 
     * @return File handler to the new config file.
     */
    protected void createConfig()
    {
        File f = new File(configPath);

        if (f.exists())
        {
            f.delete();
        }

        try
        // We should be so solid that trying is simply a formality...
        {
            f.createNewFile();
        } catch (IOException e)
        {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }

        Properties p = new Properties();

        // TODO: Change these to constant references instead.
        p.setProperty(CONFIG_DBPORT, "3306");
        p.setProperty(CONFIG_SERVERMODE, "active");
        p.setProperty(CONFIG_DBHOST, "localhost");
        p.setProperty(CONFIG_DBUSER, "giraf_web");
        p.setProperty(CONFIG_DBPASS, "cookie");
        p.setProperty(CONFIG_DB, "girafplace");
        p.setProperty(CONFIG_SERVERPORT, "2093");

        try
        {
            p.store(new FileOutputStream(f), "Default values ensue!");
            log.info("A new config file was written to the path '"
                    + f.getAbsolutePath() + "'.");
        } catch (FileNotFoundException e)
        {
            log.severe("The config file could not be found for writing. This should NOT happen!!!");
            e.printStackTrace();
        } catch (IOException e)
        {
            log.severe("An error occurred while saving the config file");
            log.severe(e.getMessage());
        } finally
        {
            loopSignal = CommandOpEnum.NEWCONF;
        }
    }

    /**
     * Checks to see if a config file exists, and responds.
     * 
     * @return True if it exists, false otherwise.
     */
    protected boolean configExists()
    {
        File f = new File(configPath);

        if (f.exists())
        {
            log.finest("Config file already exists.");
            return true;
        } else
        {
            log.finest("Config file does not exist.");
            return false;
        }
    }

    protected void _createLogger()
    {
        log = Logger.getLogger("sw3server");

        Handler[] handlers = log.getHandlers();
        if (handlers.length > 2)
        {
            log.warning("The number of active handlers exceeds 1.");
        }
        for (Handler handler : handlers)
        {
            log.removeHandler(handler);
        }

        ConsoleHandler myHandler = new ConsoleHandler();
        myHandler.setFormatter(new LoggingFormatter());
        log.addHandler(myHandler);
        log.setLevel(Level.ALL);
        log.setUseParentHandlers(false);
        myHandler.setLevel(Level.ALL);
    }
    
    /**
     * This is a helper thread for the Server class that
     * simply listens for incoming connections, assigns them
     * a new socket instance and sets them off on their good
     * tides, while logging the occurrences as necessary.
     * @author Johannes Lindhart Borresen
     */
    private class SocketThread extends Thread
    {
        /**
         * Simple bool to determine whether we're shutting down.
         */
        protected boolean isDone = false;
        
        /**
         * Signals the thread to shut down after its next iteration.
         * Note that this will *not* shut down the existing connections.
         * They can be terminated one by one within the Server.
         */
        public void close()
        {
            log.info("Closing network socket.");
            isDone = true;
            try
            {
                if (serverSocket != null)
                    serverSocket.close();
            }
            catch (IOException e)
            {
                log.warning("Failed to properly close the network socket:\n" + e.getMessage());
            }
        }
        
        @Override
        public void run()
        {
            log.fine("Now listening for connections.");
            while (!isDone)
            {
                try
                {
                    // log.info("Finding connection...");
                    Socket client = serverSocket.accept();
                    
                    // Once we get an inbound connection, we switch it on.
                    ConnectionThread ct = new ConnectionThread(client, ServerThread.this);
                    
                    connections.add(ct);
                    ct.start();
                }
                catch (SocketException e)
                {
                    log.finest("Socket timeout. Restarting.");
                }
                catch (IOException e)
                {
                    // log.warning("Could not accept connection from client:\n" + e.getMessage());
                    // We've gotta assume a Socket Exception for the time being.
                    
                }
            }
            log.info("Listener shutting down.");
        };

    }
}
