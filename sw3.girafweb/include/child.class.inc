<?php

require_once(INCDIR . "/record.class.inc"); // Base class.
require_once(INCDIR . "/constants.inc"); // Table constants.
require_once(INCDIR . "/device.class.inc");

/**
 * Data container class for a child in the system.
 */
class GirafChild extends GirafRecord
{
    /**
     * Gets the first device associated with the child.
     * \return A GirafDevice associated with the child, or null.
     * \todo{Write the function}
     */
    public function getDevice()
    {
        return null;
    }
    
    /**
     * Retrieves all devices associated with the child.
     * \param $returnAs Return type.
     * \return Array of devices of the requested type.
     * */
    public function getDevices($returnAs = GirafRecord::RETURN_PRIMARYKEY)
    {
		// Thanks to new architecture, we proxy this downwards. Woot!
		return self::getChildRecords("GirafDevice", $returnAs);
	}
    
    public static function getPrimaryKey()
    {
        return "profileId";
    }
    
    public static function getSourceTable()
    {
        return TABLE_CHILDREN;
    }
    
    public static function setAssociations()
    {
		/**
		 * Children are associated downwards with devices (and thus
		 * applications) and upwards with groups *and* users.
		 **/
		self::addAssociation("GirafDevice", TABLE_DEVICE, "ownerId", GirafDevice::getPrimaryKey());
		self::addAssociation("GirafUser", TABLE_CHILD_USER_KEYS, "childKey", "userKey");
		self::addAssociation("GirafGroup", TABLE_CHILD_GROUP_KEYS, "childKey", "groupKey");
	}
    
    /**
     * Creates a new GirafChild instance with the data found under the given
     * id.
     * \param childId ID of the child we're retrieving.
     */
    public static function getGirafChild($childId)
    {
        return parent::getInstance($childId);
    }
	
	/**
	 * Proxy for GirafRecord::getRecords. Retrieves an array of
	 * children.
	 * \param $cond Optional restrictive condition.
	 * \param $returnAs Desired return type.
	 * \return Array of children, either primary keys (ints) or objects.
	 * */
	public static function getGirafChildren($cond = null, $returnAs = self::RETURN_PRIMARYKEY)
	{
		return parent::getRecords($cond, $returnAs);
	}
	
	public function getFirstName()
	{
		$name = $this->profileName;
		
		$name = substr($name, 0, strpos($name, " "));
		
		return $name;
	}
	
	/**
	* Getting an array of abilities
	* \param The child's id
	* \return An array of bool where the index is the ability 
	*/
	public function getChildsAbilities($childId)
	{
		$result = sql_helper::selectQuery("SELECT definitionName FROM ". TABLE_KIDSPROFILE_ABILITIES ." WHERE profileKey=$childId");
		$allArray = GirafChild::getAllAbilities();
        if(!$result) return null; //error
		
		$resultArray = Array();
		$iter=1;		
		while ($row = $result->fetch_assoc())
		{
			$resultArray[$iter] = $row["definitionName"];
			$iter++;
		}
		
		foreach($allArray as $key=>$value)
		{
			$isInArray= 0;
			$isInArray = array_search($key, $resultArray);
			if(!($isInArray==false))
			{
				$allArray[$key] = true;
			}
		}

		return $allArray;
	}
	/**
	* Gets an array with abilities where the ability is the index and the value is at default false
	* \return All abilities in an array where the ability is the index and the value is at default false; 
	*/
	public static function getAllAbilities()
	{
		$result = sql_helper::selectQuery("SELECT * FROM ". TABLE_ABILITY_DEFINITIONS);
	
        if(!$result) return null; //error
		
		$resultArray = Array();    
		while ($row = $result->fetch_assoc())
		{

			$resultArray[$row["definitionName"]] = 0;
		}

		return $resultArray;
	}
	
	/**
	* Saves the changes for the child's abilities
	* \param ID: the child's id
	* \param oldAbilities: the array before changes was made
	* \param newAbilities: the array after changes was made 
	* \return if successful the return value is true and false otherwise
	*/
	public static function commitAbilityChange($ID, $oldAbilities, $newAbilities)
	{
		$aDQuery = sql_helper::selectQuery("SELECT * FROM ". TABLE_ABILITY_DEFINITIONS);
		if(!$aDQuery) return false; //error
		
		$abilityDefinition = Array();    
		while ($row = $aDQuery->fetch_assoc())
		{
			$abilityDefinition[$row["definitionId"]] = $row["definitionName"];
		}

		foreach($newAbilities as $key=>$value)
		{
			if(!($oldAbilities[$key] == $newAbilities[$key]))
			{
				$definitionKey = array_search($key, $abilityDefinition);
				if($newAbilities[$key]==true)
				{
					$result = sql_helper::insertQuery("INSERT INTO " . TABLE_PROFILE_ABILITIES . " (profileKey, definitionKey) VALUES ('$ID', '$definitionKey')");
				}
				else
				{
					$result = sql_helper::deleteQuery("DELETE FROM " . TABLE_PROFILE_ABILITIES . " WHERE profileKey = " . $ID ." AND definitionKey = " .$definitionKey . "");
					
				}
				if(!$result) return null; //error
			}
		}
		return true;
	}

}

?>
