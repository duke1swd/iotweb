#
# This code does tests on the outlet controller
#
# For each outlet-control device found cycle through a set of tests
#
# Test flow:
#    Each devices is in one of these states as it is tested:
#
#	:new		We found it, but haven't done anything to it yet.
#
#	:set		We've set it "on".  Check for On with reason remote.
#			Then set a time to go off
#
#	:wait_on	We sent a command to the device to turn the LED on.
#			we are expecting it to turn off again in < 3 seconds
#
#	:done		Done testing.  Either passed or failed.

class Outlet

def initialize
	@devicebase = 'outlet-control'
	@needtofind = true
	@mydevices = Hash.new()  # holds the state of each device
	@testsfailed = 0;
	@epoch = Time.new(2018,11,1).to_i
end

def name
	return "OutletControl"
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
				failed(name())
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

		# look to find relay state
		on = stateHash[device]['outlet/on']
		puts "\ton = #{on}" if $debug
		if on.nil?
			devstate[:state] = :done
			failed(name())
			printf ": failed to find relay state\n"
			@testsfailed += 1
				puts stateHash[device] #xxx
			next
		end

		# cycle test through various states.
		case devstate[:state]
			when :new
				puts "\t:new" if $debug
				if on == "false"		# if device already off skip to next state
					devstate[:state] = :off
					devstate[:statetime] = Time.new().to_i
					testsrunning += 1
				else
					# command the relay off
					publish("devices/#{device}/outlet/on/set", "false");
					devstate[:state] = :set_to_off
					devstate[:statetime] = Time.new().to_i
					testsrunning += 1
				end

			when :set_to_off
				puts "\t:set_to_off" if $debug

				if on != "false"
					if Time.new().to_i - devstate[:statetime] > 1
						devstate[:state] = :done
						failed(name())
						puts ": relay failed to come off within 1 second"
						@testsfailed += 1
					else
						testsrunning += 1
					end

				else
					devstate[:state] = :off
					devstate[:statetime] = Time.new().to_i
					testsrunning += 1
				end

			when :off
				publish("devices/#{device}/outlet/on/set", "true");
				devstate[:state] = :set_to_on
				devstate[:statetime] = Time.new().to_i
				testsrunning += 1
				
			when :set_to_on
				puts "\t:set_to_on" if $debug

				if on != "true"
					if Time.new().to_i - devstate[:statetime] > 1
						devstate[:state] = :done
						failed(name())
						puts ": relay failed to come on within 1 second"
						@testsfailed += 1
					else
						testsrunning += 1
					end

				else
					devstate[:state] = :time_off
					devstate[:statetime] = Time.new().to_i
					testsrunning += 1
				end

			when :time_off
				puts "\t:time_off" if $debug

				# tell the device to turn off in 5 seconds
				t = Time.new().to_i
				publish("devices/#{device}/outlet/time-off/set", (t+5-@epoch).to_s);
				devstate[:state] = :wait_off
				devstate[:statetime] = t
				testsrunning += 1

			when :wait_off
				puts "\t:wait_off" if $debug

				if on == "true"
					if Time.new().to_i - devstate[:statetime] > 6
						devstate[:state] = :done
						failed(name())
						puts ": relay failed to come off within 1 second of commanded time"
						@testsfailed += 1
					else
						testsrunning += 1
					end

				else
					t = Time.new().to_i
					publish("devices/#{device}/outlet/time-on/set", (t+5-@epoch).to_s);
					devstate[:state] = :wait_on
					devstate[:statetime] = t
					testsrunning += 1
				end


			when :wait_on
				puts "\t:wait_on" if $debug

				if on != "true"
					if Time.new().to_i - devstate[:statetime] > 6
						devstate[:state] = :done
						failed(name())
						puts ": relay failed to come on within 1 second of commanded time"
						@testsfailed += 1
					else
						testsrunning += 1
					end

				else
					# we are done!
					devstate[:state] = :done
					devstate[:statetime] = Time.new().to_i
				end

			else
				puts "\tunknown device test state #{devstate[:state]}" if $debug
		end
	end

	return :test_running if testsrunning > 0

	if @testsfailed == 0
		passed(name())
		print("\n")
	end

	return :test_done
end

end #end of class
