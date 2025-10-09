//===============thang Converyor===============

if ?.name = "AirST7"
	if @.name ="Tray2" or @.name = "Tray3"
		?.entranceLocked:=true
		?.exitLocked:=true

		//Lift2 from 0 to 0.4 meters and the speed is 0.5 m/s
		wait ?._3D.playTranslation([0,0,0],[0,0,0.4],0.5)
		//let the part exit
		?.exitLocked:=false


		waituntil ?.empty

		//down
		wait ?._3D.playTranslation([0,0,0.4],[0,0,0],0.5)

		?.entranceLocked:=false

	else
		?.entranceLocked:=true
		?.exitLocked:=true
		?.exitLocked:=false
		waituntil ?.empty

		?.entranceLocked:=false
	end
end






//===============thang Converyor xuống===============
//close entrance and exit

if ?.name = "AirST8"
	if @.name ="Tray2" or @.name="Tray3"
		?.entranceLocked:=true
		?.exitLocked:=true

		//Lift2 from 0 to 0.4 meters and the speed is 0.5 m/s
		wait ?._3D.playTranslation([0,0,0],[0,0,-0.5],0.5)

		//let the part exit
		?.exitLocked:=false
		@.transfer(AirST9)

		//down


		?.entranceLocked:=false
		wait ?._3D.playTranslation([0,0,0],[0,0,0],0.5)
	else
		@.transfer(AirST92)
	end



end



//===============Điều khiển Connect dây gián tiếp===============


param SensorID: integer, Front: boolean, BookPos: boolean
if @.name ="Tray3"
	@.Destination:=AirST113
elseif @.name ="Tray2"
	@.Destination:=AirST1121
else
	debug
end
@.AutomaticRouting:=true



//===============Tạo Part===============


param SensorID: integer, Front: boolean, BookPos: boolean
@.stopped:=true
wait 1
.UserObjects.PartTray3.create(@)
@.stopped:=false

//===============Thư viện lift===============

param SensorID: integer, Front: boolean, BookPos: boolean

@.Stopped := true;
while current.NextPred /= ?
	waituntil current.NextPred = VOID
	if current.CurrentPred = VOID
		current.NextPred = ?
	else
		if current.CurrentPred /= ?
			current.NextPred = ?
		else
			wait 1
		end
	end
end

if current.CurrentPred = VOID
	current.CurrentPred := current.NextPred
	current.NextPred := VOID
end
waituntil current.IsAvailable and current.Conveyor.Empty
current.IsAvailable := false
current.CurrentPred := ?
current.MoveObject(?)
@.Stopped := false;




//===============Input===============

AirST6.Speed = Input["Speed",1]
AirST4.Speed = Input["Speed",2]
AirST.Speed = Input["Speed",3]
AirST2.Speed = Input["Speed",4]
AirST11.Speed = Input["Speed",5]
AirST111.Speed = Input["Speed",6]
AirST22.Speed = Input["Speed",7]
AirST7.Time=Input["Time",8]


//===============Stock===============

.UserObjects.Tray2.create(AirST221)
.UserObjects.PartA.create(AirST221.cont,1)

//===============ShifCalenda===============

for var i :=1 to .UserObjects.AirST.numChildren
    .UserObjects.AirST.childNo(i).shiftCalendarObject:="ShiftCalendar"
next



//===============Đếm số tray===============

Tạo số Varialble trc tên Tray1_counter

if @.name = "Tray1"
    Tray1_counter += 1
end


