#
# This test program updates firmware on a device using OTA.
# Options:
#  -l list the firmware on all devices
#  -l -d <dev> list the firmware on one device
#  -l -f <file name of firmware> verify the file exists and print its checksum
#  -u -d <dev> -f <file name of firmware>
#  -D enable debugging
#

require 'mqtt'
require 'timeout'
require 'json'
require 'digest'

def listOfDevices()
	returnHash = Hash.new
	host = "localhost"
	c = MQTT::Client.connect(host)
	c.subscribe('devices/#')
	begin
	while true do
		Timeout::timeout(1) do
		    topic,message = c.get()
		    #puts "#{topic}: #{message}"
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

debug = false
mode = 0 #unknown
lookingfordev = false
lookingforfile = false
dev = nil
filename = nil
ARGV.each do |arg|
	if lookingfordev
		lookingfordev = false
		dev = arg
		next
	end

	if lookingforfile
		lookingforfile = false
		filename = arg
		next
	end

	case arg
	when '-D' then
		debug = true
		puts "Debugging Enabled"
		next

	when '-d' then lookingfordev = true

	when '-f' then lookingforfile = true

	when '-l' then mode = 'l' # list mode

	when '-u' then mode = 'm' # upgrade mode

	else
		puts("UNKNOWN FLAG: ${arg}")
		exit
	end
end

if mode == 0
	puts "Usage: #{$0} -(l|u) [-f firmeware file] [-d device-id]"
	exit
end

puts "Mode is #{mode}" if debug

puts "File is #{filename}" if !filename.nil?
puts "Device is #{dev}" if !dev.nil?
	
allDevices = listOfDevices

# debugging routine to print what we got from mqtt
if debug
	puts "Device List:"
	allDevices.each do |device,devHash|
		puts "\t" + device
		devHash.each do |topic,message|
			if topic == '$implementation/config'
				puts "\t\t#{topic}:"
				JSON.pretty_generate(JSON.parse(message)).each_line do |line|
					puts "\t\t\t#{line}"
				end
			else
				puts "\t\t#{topic}:  #{message}"
			end
		end
	end
	puts ""
end

# If a device was specified see if we know anything about it.
if (not dev.nil?) && allDevices[dev].nil?
	puts "Cannot find device #{dev} on MQTT"
	exit
elsif not dev.nil?
	puts "Device #{dev} firmware is #{allDevices[dev]['$fw/checksum']}"
end

# If a filename was specified, get its MD5 checksum
if not filename.nil?
	begin
		checksum = Digest::MD5.file filename
		puts "File #{filename} has checksum #{checksum}"
	rescue Exception => bang
		puts "Exception #{bang}"
		exit
	end
end
