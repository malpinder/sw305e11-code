<script>
var lastModuleUrl = "";

function getChildIdFromAppButton(fullId)
{
	return fullId.substring(0, fullId.indexOf("-"));
}

function getAppIdFromAppButton(fullId)
{
	return fullId.substring(fullId.lastIndexOf("-") + 1);
}

/**
 * This function is used as a callback to AJAX calls when loading
 * sub modules (subcontrollers) for applications tied to single
 * children.
 * (guess what we use ignore and ignoreToo for).
 */
function printModule(contents)
{
	// Fill window.
	$("#window").html(contents);
}

function refreshModule()
{
	$.get(lastModuleUrl, {}, printModule);
}

$(document).ready(function(){
	$(".app-item").hide();
	
	$(".child-item").click(function()
	{
		var cId = event.target.id;
		var childId = cId.substring(cId.lastIndexOf("-")+1);
		var name = ".app-for-child-" + childId;
		// console.debug(name);
		
		// Pretty, but glitchy.
		// $(".app-select").slideUp('fast', function(){$(".for-child-" + childId).slideDown('fast');});
		
		// Functional, but poppy.
		$(".app-item").hide();
		$(name).show();;
	});
	
	$(".app-item").click(function()
	{
		var cId = event.target.id;
		// console.debug(cId);
		var appId = getAppIdFromAppButton(cId);
		var kidId = getChildIdFromAppButton(cId);
		
		// alert(appId + " and " + kidId);
		// $.get("themes/default/cbMessages.tpl.php", { child : kidId}, printModule);
		// var gUrl = "<?=BaseUrl()?>module/"+appId+"/"+kidId;
		lastModuleUrl = "<?=BaseUrl()?>module/"+appId+"/"+kidId;
		// console.debug(gUrl);
		refreshModule();
	});
});</script>
<div id="site-box">
<div id="header">
	<h1 id="GIRAFTitle">GIRAF v1.0a</h1>
	<div id="accountbox">
		<li>My Account</li>
		<li><a href="index.php?page=login&action=logout">Log out</a></li>
	</div>
</div>
<div id="menu">
	<div id="childlist">
		<?php foreach($kids as $child){	
			$c = $child->id;
			?>
			<div class="menu-item child-item" id="child-<?=$c?>">
				<div class="image-div" id="image-div-<?=$c?>">
					<img class="menu-image" id="image-<?=$c?>" src="<?=BaseUrl() . '/img/profile-photo.jpg'?>" />
				</div>
				<div class="menu-item-title" id="name-<?=$c?>">
					<?=$child->getFirstName()?>
				</div>
			</div>
		<?php } ?>
	</div>
	<div id="applist">
		<?php
		
		// PHP: print out all apps with class/id that determines child.
		// js: hide/show depending on picked child.
		
		foreach($kidsApps as $kidId => $apps)
		{
			foreach ($apps as $app)
			{
				$an = $app->applicationName;
				$ai = $app->id;
			?>
			<div class="menu-item app-item app-<?=$ai?> app-for-child-<?=$kidId?>" id="<?="$kidId-$ai"?>">
				<span id="<?="$kidId-$ai"?>"><?=$an?></span>
			</div>
			<?php
			}
		}
		
		?>
	</div>
</div>

<div id="window">
	<!-- ?="No module defined!"? -->
</div>
</div>
