#
# This test program removes obsolete persistent messages.
#
# Usage:
#	ruby clearn.rb [-f] <prefix string>
# Options:
#  -f actually remove.  Otherwise just lists

require 'mqtt'
require 'timeout'

$host = "localhost"

$debug = false
$mode = "l"	# default is list only
looking = true
$prefix = nil
$limitxxx = 64

def clean_topic(topic)
	puts("Cleaning: #{topic}") if $debug

	begin
		MQTT::Client.connect($host) do |c|
			c.publish(topic, "", true)
		end

	rescue Exception => bang
		puts "Exception during clean-up publish"
		puts "Exception #{bang}"
		exit
	end
end

ARGV.each do |arg|
	case arg
	when '-D' then
		$debug = true
		puts "Debugging Enabled"
		next

	when '-f' then $mode = "f"

	else
		if looking
			$prefix = arg
			looking = false
			puts("Prefix is #{$prefix}") if $debug
		else
			puts("UNKNOWN FLAG: #{arg}")
			exit
		end
	end
end

if looking
	puts("Usage: ruby clean.rb [-f] <prefix>") 
	exit
end

c = MQTT::Client.connect($host)

c.subscribe($prefix + '/#')
begin
while true do
	Timeout::timeout(1) do
	    topic,message = c.get()
	    if topic.length > 0
		puts "Topic length is #{topic.length}" if $debug
		if message.include?("firmware")
			puts "#{topic}: (suppresed)"
		else
			puts "#{topic}: #{message}"
		end
		clean_topic(topic) if $mode == "f"
		$limitxxx = $limitxxx - 1
		if $limitxxx <= 0
			puts "Exit on limit" if $debug
			exit 
		end
	    end
	end
end
rescue Timeout::Error
	# Ignore the timeout error, just means we are not waiting for more.
	# Mostly this is a hack to get the persistent messages only.
end

c.disconnect
exit
