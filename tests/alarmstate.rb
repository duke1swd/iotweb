#
# This code does a simple test on the alarm state sensor
#
# For each alarm-state device found, send it an ON message
# and see that after a while it turns off.
#

# Test flow:
#    Each devices is in one of these states as it is tested:
#
#	:new		We found it, but haven't done anything to it yet.
#
#	:led_set	LED has been commanded on.  Must come on.
#
#	:led_on		We sent a command to the device to turn the LED on.
#			we are expecting it to turn off again in < 3 seconds
#
#	:done		Done testing.  Either passed or failed.

class AlarmState

def initialize
	@devicebase = 'alarm-state'
	@needtofind = true
	@mydevices = Hash.new()  # holds the state of each device
	@testsfailed = 0;
end

def name
	return "AlarmState"
end

def run(stateHash)
	if @needtofind
		@needtofind = false
		(0..99).each do  |seqn|
			device = sprintf("%s-%04d", @devicebase, seqn)

			# Does this device exist in system state?
			next if (deviceHash = stateHash[device]).nil?
			puts "Found #{device}" if $verbose

			if deviceHash['$online'].nil? or deviceHash['$online'] != "true"
				failed("AlarmState")
				puts "Device #{device} is not on line"
				next
			end
			@mydevices[device]= {state: :new, statetime: 0}
		end
		return :test_running
	end
	
	puts "Testing these devices:" if $debug
	testsrunning = 0
	@mydevices.each_pair do |device,devstate|
		puts "\t#{device}" if $debug

		# look to find LED state
		on = stateHash[device]['on']
		set = stateHash[device]['set']
		puts "\ton = #{on}, set = #{set}" if $debug
		if on.nil? or set.nil?
			devstate[:state] = :done
			failed("AlarmState")
			printf ": failed to find LED state\n"
			@testsfailed += 1
				puts stateHash[device] #xxx
			next
		end

		# cycle test through various states.
		case devstate[:state]
			when :new
				puts "\t:new" if $debug
				# command the LED on
				publish("devices/#{device}/led/on/set", "true");
				devstate[:state] = :led_set
				devstate[:statetime] = Time.new().to_i
				testsrunning += 1

			when :led_set
				puts "\t:led_set" if $debug
				if set != "true"
					failed("AlarmState")
					printf ": failed to set LED state\n"
					@testsfailed += 1
					next
				end

				if on == "true"
					devstate[:state] = :led_on
					devstate[:statetime] = Time.new().to_i
					testsrunning += 1
					next
				end

				if Time.new().to_i - devstate[:statetime] > 1
					devstate[:state] = :done
					failed("AlarmState")
					printf ": LED failed to come on within 1 second\n"
					@testsfailed += 1
				else
					testsrunning += 1
				end

			when :led_on
				puts "\t:led_on" if $debug
				if on == "false" and set == "false"
					devstate[:state] = :done	# passed!
				elsif Time.new().to_i - devstate[:statetime] > 3
					devstate[:state] = :done
					failed("AlarmState")
					printf ": LED on for more than 3 seconds\n"
					@testsfailed += 1
				else
					testsrunning += 1
				end
			else
				puts "\tunknown device test state #{devstate[:state]}" if $debug
		end
	end

	return :test_running if testsrunning > 0

	if @testsfailed == 0
		passed("AlarmState")
		print("\n")
	end

	return :test_done
end

end #end of class
