#
# This test program updates firmware on a device using OTA.
# Options:
#  -l list the firmware on all devices
#  -l -d <dev> list the firmware on one device
#  -l -f <file name of firmware> verify the file exists and print its checksum
#  -u -d <dev> -f <file name of firmware>
#  -D enable debugging
#  -F clear ota cruft and reset the device
#

require 'mqtt'
require 'timeout'
require 'json'
require 'digest'

@host = "localhost"
@maxfilesize = 512 * 1024 * 1024	# half a gigabyte
#firmwareat = ""

def listOfDevices()
	returnHash = Hash.new
	c = MQTT::Client.connect(@host)
	c.subscribe('devices/#')
	begin
	while true do
		Timeout::timeout(1) do
		    topic,message = c.get()
		    #puts "#{topic}: #{message}"


		    # ignore any broadcast messages lurking about
		    next if /\$broadcast/ =~ topic

		    # pull the device name out of the message topic
		    device = topic.sub(/devices\/([a-zA-Z0-9\-]+)\/.*/, '\1')
		    if ! returnHash[device]
		    	returnHash[device] = Hash.new
		    end

		    # get the rest of the topic, including sub topics.
		    deviceTopic = topic.sub(/devices\/#{device}\/(.*)/, '\1')
		    returnHash[device][deviceTopic] = message
		end
	end
	rescue Timeout::Error
		# Ignore the timeout error, just means we are not waiting for more.
		# Mostly this is a hack to get the persistent messages only.
	end

	c.disconnect
	return returnHash
end

def otapublish(key, message)
	topic = 'devices/' + @dev + '/$implementation/ota/' + key
	m = (/firmware/ =~ topic)? 'binary': message
	puts "Publishing #{topic} <= #{m}" if @debug
	begin
		MQTT::Client.connect(@host) do |c|
			c.publish(topic, message, true)
		end

	rescue Exception => bang
		puts "Exception during OTA publish"
		puts "Exception #{bang}"
		exit
	end
end

#
# Get rid of old persistent OTA messages
#
def clean_up
	otapublish(@firmwareat, "")
	otapublish('status', "")
end

#
# read the firmware file and publish it
#
def publish_firmware(checksum)
	File.open(@filename, "r") do |f|
		size = f.size
		if size > @maxfilesize
			puts "File #{@filename} is too big"
			exit
		end
		firmware = f.read(size)
		@firmwareat = 'firmware/' + checksum.hexdigest
		otapublish(@firmwareat, firmware)
	end
end
	

@debug = false
mode = nil #unknown
lookingfordev = false
lookingforfile = false
@dev = nil
@filename = nil
ARGV.each do |arg|
	if lookingfordev
		lookingfordev = false
		@dev = arg
		next
	end

	if lookingforfile
		lookingforfile = false
		@filename = arg
		next
	end

	case arg
	when '-D' then
		@debug = true
		puts "Debugging Enabled"
		next

	when '-F' then mode = 'F'

	when '-d' then lookingfordev = true

	when '-f' then lookingforfile = true

	when '-l' then mode = 'l' # list mode

	when '-u' then mode = 'm' # upgrade mode

	else
		puts("UNKNOWN FLAG: #{arg}")
		exit
	end
end

if mode.nil?
	puts "Usage: #{$0} -(l|u) [-f firmeware file] [-d device-id]"
	exit
end

puts "Mode is #{mode}" if @debug

puts "File is #{@filename}" if !@filename.nil? && @debug
puts "Device is #{@dev}" if !@dev.nil? && @debug
	
allDevices = listOfDevices

# debugging routine to print what we got from mqtt
if @debug
	puts "Device List:"
	allDevices.each do |device,devHash|
		puts "\t" + device
		devHash.each do |topic,message|
			if topic == '$implementation/config'
				puts "\t\t#{topic}:"
				JSON.pretty_generate(JSON.parse(message)).each_line do |line|
					puts "\t\t\t#{line}"
				end
			elsif /firmware/ =~ topic
				puts "\t\t#{topic}:  <binary>"
			else
				puts "\t\t#{topic}:  #{message}"
			end
		end
	end
	puts ""
end

# If a device was specified see if we know anything about it.

if (not @dev.nil?) && allDevices[@dev].nil?
	puts "Cannot find device #{@dev} on MQTT"
	exit
elsif not @dev.nil?
	puts "Device #{@dev} firmware:"
	puts "\tname:\t\t#{allDevices[@dev]['$fw/name']}"
	puts "\tversion:\t#{allDevices[@dev]['$fw/version']}"
	puts "\tchecksum:\t#{allDevices[@dev]['$fw/checksum']}"
end

# If a filename was specified, get its MD5 checksum

if not @filename.nil?
	if mode == 'F' then
		puts "(F)orce mode does not take a file name (#{@filename})"
		exit
	end
	begin
		checksum = Digest::MD5.file @filename
		puts "File #{@filename} has checksum #{checksum}"
	rescue Exception => bang
		puts "Exception during digest calculation"
		puts "Exception #{bang}"
		exit
	end
end

# If we've been asked for a list of devices with their firmware, print that
if mode == 'l' and @dev.nil? and @filename.nil?
	allDevices.each do |device,devHash|
		c = devHash['$fw/checksum']
		c = "unknown" if c.nil?
		puts "\t" + device + "\t" + c
	end
	puts ""
end

exit if mode == 'l'

if mode == 'F'

	if @dev.nil?
		puts "Force mode requires a device to force clear"
		exit
	end

	# clear out old messages
	clean_up

	# now reset the device
	# (don't know how to do this)
	exit
end

#
# Come here to do an upgrade
#
# First, some sanity checks
#
if @filename.nil? or @dev.nil?
	puts "upgrade mode requires both a file and a device"
	exit
end

if checksum == allDevices[@dev]['$fw/checksum']
	puts "Device #{@dev} is already running firmware #{@filename}"
	exit
end

if allDevices[@dev]['$online'] != "true"
	puts "Device #{@dev} is not online"
	exit
end

# check if OTA status already posted by the device
status = allDevices[@dev]['$implementation/ota/status']
if not status.nil?
	puts "Device #{@dev} is showing OTA status #{status} before OTA starts"
	exit
end

#
# Kick things off by publishing the firmware to upload.
#

publish_firmware(checksum)

#
# This loop waits on status messages, and depending on message
# does things.
#
@lastmess = nil
nextpct = 5
begin
	c = MQTT::Client.connect(@host)
	c.subscribe('devices/' + @dev + '/$implementation/ota/status')
	begin
	    while true do
		Timeout::timeout(1) do
			topic,message = c.get()
		next if message == @lastmess
		@lastmess = message
		puts "\tGot status message " + message if @debug

		status = message.sub(/^([0-9]+).*/, '\1').to_i
		puts "Status code is #{status}" if @debug

		if status == 202
			puts "Firmware Accepted"
			puts "\t" + message if @debug
			next
		end
		if status == 206
			puts "\t" + message if @debug
			pct = message.sub(/^[0-9]+ ([0-9]+)\/.*/, '\1').to_f /
			    message.sub(/^[0-9]+ [0-9]+\/([0-9]+)/, '\1').to_f 
			if pct * 100 >= nextpct
				print "*"
				nextpct = nextpct + 5 #print a * every 5%
			end
			next
		end
		if status == 200
			puts "  Firmware Load Successful"
			clean_up
			exit
		end
		if status == 304
			puts "Attempted to upload already running firmware.  Aborting."
			clean_up
			exit
		end
		if status == 400
			puts "Device reports OTA Bad Checksum or Firmware.  Aborting."
			puts message
			clean_up
			exit
		end
		if status == 403
			puts "OTA not enabled on this device.  Aborting."
			clean_up
			exit
		end
		if status == 500
			puts "Device reports internal error.  Aborting."
			puts message
			clean_up
			exit
		end
		puts "Initial OTA status is #{message}.  Aborting"
		clean_up
		exit
	    end
	end
	rescue Timeout::Error
	    puts "Timeout Error during upload"
	    exit
	end

rescue exit
rescue Exception => bang
	puts "Exception in upload state machine" if @debug
	puts "Exception #{bang}"
	exit
end
