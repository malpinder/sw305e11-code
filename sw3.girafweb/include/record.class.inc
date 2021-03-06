<?php

require_once(__DIR__ . "/sql_helper.inc");
require_once(__DIR__ . "/user.class.inc");

/**
 * The GirafRecord class is the base class for all data containers in the Giraf
 * web system. It contains automated facilities for retrieving field lists and
 * handling changes in them like record sets in ODBC. This unified inheritance
 * between all table-sourced container classes should simplify maintenance and
 * extension.
 * \todo Implement static initialisation of all subclasses of GirafRecord, so
 * their primary keys and table names can be accessed without an instance.
 */
abstract class GirafRecord implements GirafRecordInterface
{
    /**
     * Used when needing a recordset returned only as primary keys.
     */
    const RETURN_PRIMARYKEY = 0;
    
    /**
     * Used when needing a recordset returned as full records.
     */
    const RETURN_RECORD = 1;
    
    /**
     * Contains the actual data from the database.
     */
    private $data;
    
    /**
     * Array of booleans that denote whether their corresponding data in the
     * $data array have been changed and thus need to be committed.
     */
    private $dirty_bits;
    
    /**
     * Name of the subclass that inherited this GirafRecord.
     */
    protected $subclass = null;
    
    /**
     * Name of the table that data comes from. Since merely changing the
     * variable does very little, it is hidden from subclasses. Use
     * setSourceTable to make a proper change.
     * \sa GirafRecord::setSourceTable()
     */
    private $sourceTable = null;
     
    /**
     * Global array of associations between classes. It's an array
     * structured as thus:
     * $classAssociations[parentClass][childClass] = GirafRecordAssociation.
     * */
    private static $classAssociations = array();
   
    /**
     * Retrieves the association that connects this GirafRecord and a
     * passed record.
     * \param $forRecord The record to find associations for. Can be
     * either a class or an object of the class.
     * \return A GirafRecordAssociation object that details how the
     * association fits. False if no such association exists.
     * */
    public static function getAssociation($forRecord)
    {
		// Figure out which class we're in.
		$pClass = get_called_class();
		
		// Get the correct subarray.
		$assocs = self::getClassAssociations();
		if ($assocs == false) return false;
		
		// See if there is an association.
		if (!array_key_exists($forRecord, $assocs)) return false;
		return $assocs[$forRecord];
	}
    
    /**
     * Retrieves all the current class' associations.
     * \return Array of associations, keyed by the child class. False
     * is returned if no associations exist.
     * */
    private static function getClassAssociations()
    {
		$n = get_called_class();
		
		if (array_key_exists(get_called_class(), self::$classAssociations))
			return self::$classAssociations[$n];
		else
			return false;
	}
    
    /**
     * Adds a new association to the current class.
     * \param $toClass Child class of the association.
     * \param $assocTable String name of the table in the database.
     * \param $pKey String name of the column for parent class key.
     * \param $cKey String name of the column for child class key.
     * */
    protected static function addAssociation($toClass, $assocTable, $pKey, $cKey)
    {
		$name = get_called_class();
		if (!is_string($toClass)) $childClass = get_class($toClass);
		else $childClass = $toClass;
		
		// Create the association array if it didn't already exist.
		if (self::getClassAssociations() == false) self::$classAssociations[$name] = array();
		
		self::$classAssociations[$name][$childClass] = new GirafRecordAssociation($assocTable, $pKey, $cKey);
	}
    
    /**
     * Retrieves a particular group of child records for the current
     * record.
     * \param $childType String name of the child type to retrieve for.
     * \param $returnAs Type to return as. Defaults to primary keys.
     * \return Array of children, type determined by $returnAs
     * parameter. False is returned if the association doesn't exist.
     * */
    public function getChildRecords($childType, $returnAs = self::RETURN_PRIMARYKEY)
    {
		// Un-committed new records can by definition not have children.
		if ($this->isNewRecord()) return false;
		
		$assocObject = self::getAssociation($childType);
		
		if ($assocObject == false) return false;
		
		$children = $assocObject->getChildIds($this->id);
		
		if ($returnAs == self::RETURN_PRIMARYKEY) return $children;
		
		// Create an empty instance for the static functions.
        $refl = new ReflectionClass($childType);
        
        // Create a new subclass instance.
        $emptyInstance = $refl->newInstance();
		
		$ret = array();
		foreach($children as $child)
		{
			$ret[] = $emptyInstance::getInstance($child);
		}
		
		return $ret;
	}
    
    /**
     * Whether the current record object is new (that is, not yet in the
     * database) or old (that is, pulled from the database.).
     * This variable will be true only when id == -1.
     */
    public function isNewRecord()
    {
        return $this->id == -1 ? true : false;
    }
    
    /**
     * Sets all the dirty bits to a boolean value.
     * \param state New state, false/true.
     */
    protected function setDirtyState($state)
    {
        $len = count($this->dirty_bits);
        for($i = 0; $i < $len; $i++)
        {
            $this->dirty_bits[$i] = $state;
        }
    }
    
    /**
     * The static constructor must be defined in subclasses and must be called
     * by the class definitions' file. There is no method of forcing the second
     * requirement, but the code will fail miserably if you try :D
     */
    protected static final function staticConstructor(){}
    
    public function __construct()
    {
		
        // echo "Constructing from " . get_called_class();
        $this->subclass = get_called_class();
        $this->setSourceTable(static::getSourceTable());
        static::setAssociations();
    }
    
    /**
     * Returns the name of the table associated with this record type. Must be
     * overridden in subclasses where they define their source table.
     */
    public static function getSourceTable()
    {
        return null;
    }
    
    /**
     * Returns the name of the primary key on the database table for this record
     * type.
     * \return The primary key's field name as it is found in the database.
     */
    public static function getPrimaryKey()
    {
        return "id";
    }
    
    /**
     * Sets the class' source table information, field data and such.
     * It should never be necessary to call this function more than once in 
     * a class' execution lifetime.
     * \param $table Name of the table to scan in.
     */
    protected function setSourceTable($table)
    {
        // Set subclass data for later instantiations. If it wasn't set, get it
        // from the class itself.
        
        /* We dynamically prep the internal data array with the exact
         * field names from the database itself.
         */
        $this->data = Array();
        $this->dirty_bits = Array();
        
        $this->sourceTable = $table;
        
        /**
         * The DESCRIBE statement is akin to a SELECT statement, only for table structure, not table data.
         */
        $result = sql_helper::selectQuery("DESCRIBE " . $table);
        
        if (!$result)
        {
            throw new Exception("The users table '" . $table . "' does not exist!");
        }
        
        $iter = 0;
        $fields = array();
        while ($row = $result->fetch_assoc())
        {
            $field = $row["Field"];
            $this->data[$field] = "";
            $this->dirty_bits[$field] = false;
        }
        
        return;
    }
    
    /**
     * Gets the data associated with a particular field.
     * \param key Name of the field you want data from.
     * \return The value associated with that field. May be null.
     * \throws Exception If an invalid key is requested, an exception is thrown.
     */
    public function __get($key)
    {
        // echo $key;
        if ($key == "id")
            return $this->data[static::getPrimaryKey()];
        // Second, check if the requested key is another field. Throw a antrum if not.
        elseif (!array_key_exists($key, $this->data))
            throw new Exception("The requested key does not exist.");
        // Finally, return the found value.
        else
            return $this->data[$key];
    }
    
    /**
     * Sets a value on the user. This will automatically also register that value to be committed to the database.
     * Note that only keys matching fields on the underlying table will be accepted.
     * \param key The key (field) to set a value to.
     * \param val The data to apply to the key.
     * \warning At this time, neither type nor writability is performed. As such, you may get erratic behaviour if you try to apply a string to the userId field (which is read-only - and an int).
     * \throws Exception Throws an exception if an invalid field is requested.
     */
    public function __set($key, $val)
    {
        // echo "Setting $key";
        if ($key == static::getPrimaryKey())
            throw new Exception("The primary key value is write-protected.");
        if (!array_key_exists($key, $this->data))
            throw new Exception("The requested key does not exist.");
        else
        {
            $this->data[$key] = $val;
            $this->dirty_bits[$key] = true;
        } 
    }
    
    /**
     * Sets the primary key value of the record. This must be a protected
     * action, as the key is considered write-protected for the lifetime of the
     * affected row.
     */
    private function setId($id)  
    {
            $this->data[static::getPrimaryKey()] = $id;
    }
    
    /**
     * Retrieves an instance of the object from the database, or an empty
     * object if not set.
     * \param $id If set, the id will be retrieved from the database. if omitted, a new object is created.
     * \return A new object of the proper subclass. If an id was requested, the data fields will be set. If not, an empty object is returned.
     */
    protected static function getInstance($id = null)
    {
        // Use reflection to get subclass info.
        $refl = new ReflectionClass(get_called_class());
        
        // Create a new subclass instance.
        $newU = $refl->newInstance();
    
        // Quick stop. If no ID was requested. Quickly create something new.
        if (!isset($id))
        {
            $newU->setId(-1);
            return $newU;
        }
        else
        {
            // Retrieve on hit matching the user.
            $result = sql_helper::selectQuery("SELECT * FROM " . static::getSourceTable() . " WHERE " . static::getPrimaryKey() . "=$id");
            
            // If an error occurred or we got no hits, return false.
            if (!$result || $result->num_rows == 0) return false;
            
            // Retrieve the result.
            $row = $result->fetch_assoc();
            
            // Insert query data into the new instance.
            foreach ($row as $key=>$val)
            {
                if($key != static::getPrimaryKey()) $newU->$key = $val;
                else $newU->setId($val);
            }
        }
        
        // Reset all the dirty bits for proper commit support.
        $newU->setDirtyState(false);
        
        // Return the new instance.
        return $newU;
    }
    
    /**
     * Commits changes made to the GirafUser back to the database. Handy for
     * changes like name, mail and the like when no further actions are needed.
     * \return true on success, false otherwise (check sql_helper::error).
     * \sa sql_helper::getErrMsg()
     * \todo Not written yet, but SqlUpdateCommand has been implemented to facilitate this.
     */
    public function commit()
    {
	// $this->primaryKey
        if ($this->data[$this->getPrimaryKey()] > 0) GirafRecord::commitUpdate();
		else GirafRecord::commitInsert();	
    }
    
    /**
     * Called by GirafRecord::commit() when the data is an update of current record.
     */
    private function commitUpdate()
    {
			
        $sql = "UPDATE " . static::getSourceTable() . " SET ";
        
        foreach ($this->dirty_bits as $name=>$isDirty)
        {
            if($isDirty)
            {
                $sql .= "$name= '" . sql_helper::escapeString($this->data[$name]) ."', ";
            }
        }
        $sql = substr($sql, 0, strlen($sql) - 2); // Remove final colon.
        
        $sql .= " WHERE " . $this->getSelfCondition();
        sql_helper::updateQuery($sql);
    }
    
    /**
     * Called by GirafRecord::commit() when the data is a new record.
     */
    private function commitInsert()
    {
        $sql = "INSERT INTO " . static::getSourceTable() . " ";
        
        $values = "";
        
        // Insert column names.
        foreach ($this->data as $name=>$val)
        {
            $sql .= "$name, ";
            $val = sql_helper::escapeString($val);
            $values .= "'$val',";
        }
        
        $sql = trim($sql, ","); // Trim final comma.
        $value = trim($value, ","); // Trim final comma.
        
        
        $sql .= " VALUES ($values)";
    
        sql_helper::insertQuery($sql);
    }
    
    /**
     * Convenience function to construct and return the condition in a WHERE
     * statement that refers specifically to the current instance's id.
     */
    protected function getSelfCondition()
    {
        return static::getPrimaryKey() . "=" . $this->id;
    }
    
    /**
     * Retrieves a set of records from the source table either by their primary
     * keys or complete copies.
     * \param $condition The condition to pull the records from. This must be a valid part of SQL's "WHERE".
     * \param $return_as The type of return type needed.
     *  Valid options are RETURN_PRIMARYKEY (which returns an array of keys) and
     *  RETURN_RECORD (which returns an array of SimpleRecord objects, each containing one record).
     * \warning On sufficiently large tables, using RETURN_RECORD (and at some point RETURN_PRIMARYKEY) <br>will</br> result in errors. Be sure to place proper restrictions with $condition beforehand.
     * \warning Since this function allows a very open condition structure, we must expect the calling environment to escape the conditions securely.
     */
    public function getRecords($condition = null, $return_as = self::RETURN_PRIMARYKEY)
    {
        // Start by getting all the keys. We'll need them regardless.
        $sql = "SELECT " . static::getPrimaryKey() . " FROM " . static::getSourceTable();
        
        // If we have a specific condition, set that up.
        if (isset($condition)) $sql .= " WHERE $condition";
        
        // var_dump($sql);
        // die();
        
        // Execute the query.
        $keys = sql_helper::selectQuery($sql);
        
        // If something bad happened, throw a tantrum.
        if (!$keys) throw new Exception(sql_helper::getErrMsg());
        
        // Insert all the keys from the result as integers into an array.
        $key_array = Array();
        $iter = 0;
        // Turn into array.
        while ($row = $keys->fetch_row())
        {
            $key_array[$iter] = intval($row[0]);
            $iter++;
        }
        
        // At this point, the primary keys are stored in a nice array.
        
        if ($return_as == self::RETURN_PRIMARYKEY) return $key_array;
        
        // If the user wanted more, run a new loop.
        $records = Array();
        $iter = 0;
        foreach ($key_array as $key)
        {
            $records[$iter] = self::getInstance($key);
            $iter++;
        }
        
        // var_dump($records);
        
        return $records;
    }
}

interface GirafRecordInterface
{
	static function setAssociations();
}

/**
 * First iteration of ?-to-? associations in the model. By defining
 * parent and child classes we implicitly get their corresponding tables
 * and columns/keys. Furthermore, we need three pieces of info. An
 * association table, and which columns in that table define parent and
 * child foreign keys, respectively.
 * */
class GirafRecordAssociation
{
	public $assocTable;
	public $parentKey;
	public $childKey;
	
	public function __construct($aTable, $pKey, $cKey)
	{
		$this->assocTable = $aTable;
		$this->parentKey = $pKey;
		$this->childKey = $cKey;
	}
	
	public function getChildIds($p)
	{
		$sql = "SELECT $this->childKey FROM $this->assocTable WHERE $this->parentKey=$p";
		
		$result = sql_helper::selectQuery($sql);
		
		if ($result === false) throw new Exception("The hell?!: " . sql_helper::getErrMsg());
		
		$ret = array();
		while ($row = $result->fetch_row()) $ret[] = $row[0];
		
		return $ret;
	}
	
	/**
	 * Method overloading to automagically support "getSubClass" methods
	 * in subclasses. This makes the semantically pleasing methods
	 * available without further implementations.
	 * \param $name Name of the method being called.
	 * \param $params Array of parameters to pass to it.
	 **/
	public static function __callStatic($name, $params)
	{
		if (!is_array($params)) throw new Exception("Parameters were not passed as array.");
		if ($name == 'get'.get_class())
		{
			return static::getInstance($params[0]);
		}
		// Final solution. Der Ent-...dinge
		return parent::__call($name, $params);
	}
}

?>
