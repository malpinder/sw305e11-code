<?php

/**
 * Simple class to aid in the maintenance of PHP sessions. This is particularly useful (not to mention necessary) when working with login functionality such as ours.
 * Note that this class is very basic, only slightly better than syntactic sugar as it simply wraps around existing session functions.
 * Note the class contains only static functions and it should be considered static itself.
 */
class GirafSession
{
    public static function __get($name)
    {
        return $_SESSION[$name];
    }
    
    public static function __set($name, $val)
    {
        $_SESSION[$name] = $val;
    }
    
    public static function getSessionId()
    {
        return session_id();
    }
    
    /**
     * Retrieves a session's data. If no session exists, a new session
     * is created.
     * \return A new GirafSession instance.
     */
    public static function getSession($id = -1)
    {
        session_start();
    }
    
    /**
     * Returns the database ID of the user currently logged into this session.
     * \return The databse ID if the user is logged in, false otherwise.
     */
    public static function getCurrentUser()
    {
        return isset($this->$userId) ? $this->$userId : false;
    }
}

?>