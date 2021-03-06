// ReZEL demo

ReZEL.set.bulletSpeed = 2;
ReZEL.set.walkSpeed = 0.05;
ReZEL.set.reloadTime = 5.;
ReZEL.set.rifleMagazineSize = 5;
ReZEL.set.vulcanReloadTime = 5.;
ReZEL.set.vulcanMagazineSize = 20;

function printeach(e){foreach(a,b in e) print(a + ": " + b);}

initUI();

warfield <- bookmarks["Side 7"].cs();
player.cs = warfield;

//deltaFormation("ReZEL", 0, Quatd(0,0,0,1), Vec3d(10, 0., 10. - 19.), 0.05, 1, player.cs, @(e) e.command("Move", Vec3d(10,0,0)));
//deltaFormation("Sceptor", 0, Quatd(0,0,0,1), Vec3d(10.01, 0., 10.01 - 20.), 0.05, 1, player.cs, null);
deltaFormation("ReZEL", 0, Quatd(0,0,0,1), Vec3d(0, 3., 3.), 0.05, 2, player.cs, null);
//deltaFormation("ReZEL", 1, Quatd(0,1,0,0), Vec3d(0, 0., 3.), 0.05, 2, player.cs, null);
//deltaFormation("ReZEL", 0, Quatd(0,0,0,1), Vec3d(0, -19., 0), 0.05, 1, player.cs, @(e) e.command("Move", Vec3d(0,10,0)));
//deltaFormation("ContainerHead", 0, Quatd(0,0,0,1), Vec3d(10, 0., 10. - 20.), 0.15, 1, player.cs, null);
//deltaFormation("Sceptor", 1, Quatd(0,1,0,0), Vec3d(0, 0., -3.), 0.05, 3, player.cs, null);

cmd("bind t \"sq \\\"player.chase.command(\\\\\\\"GetCover\\\\\\\", 2)\\\"\"");
cmd("bind r \"sq \\\"player.chase.command(\\\\\\\"Reload\\\\\\\")\\\"\"");
cmd("pause 0");
player.setrot(Quatd(0,0,0,1)); // Reset rotation for freelook
player.setmover("tactical");

local lookrot = Quatd(0., 1., 0., 0.) * Quatd(-sin(PI/8.), 0, 0, cos(PI/8.)) * Quatd(0, sin(PI/8.), 0, cos(PI/8.));
player.setpos(Vec3d(10,0,10-16));
player.setrot(lookrot);
player.viewdist = 0.25;


function frameproc(dt){
	for(local i = 0; i < 2; i++){
		if(countents(warfield, i, "ReZEL") < 2){
			local axis = rand() % 3 * 2. - 2.;
			local phase = rand() % 3 * 2. * PI / 3.;
			deltaFormation("ReZEL", i, Quatd(0,0,0,1) * Quatd.rotation(phase, Vec3d(0,1,0)),
				Vec3d(3. * sin(phase), 3. * (1. - i) + axis, 3. * cos(phase)), 0.05, 2, warfield, null);
		}
	}
}

ReZEL_deaths <- [0,0];

try{
	local f = file("ReZEL_deaths", "rb");
	for(local i = 0; i < 2; i++){
		ReZEL_deaths[i] = f.readn('i');
		print("ReZEL death[" + i + "]: " + ReZEL_deaths[i]);
	}
}
catch(e){
	print("Could not load ReZEL_deaths: " + e);
}

function hook_delete_Entity(e){
	if(e.classname == "ReZEL"){
		ReZEL_deaths[e.race]++;
		print("ReZEL death[" + e.race + "]: " + ReZEL_deaths[e.race]);
		local f = file("ReZEL_deaths", "wb");
		for(local i = 0; i < 2; i++)
			f.writen(ReZEL_deaths[i], 'i');
	}
}

function sqtimechart(){
	return ReZEL_deaths[0];
}

