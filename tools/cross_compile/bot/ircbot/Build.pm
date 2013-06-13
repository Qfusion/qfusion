package Bot::BasicBot::Pluggable::Module::Build;
BEGIN {
  $Bot::BasicBot::Pluggable::Module::Build::VESION = '0.10';
}
use base qw(Bot::BasicBot::Pluggable::Module);
use warnings;
use strict;
use File::Basename;

sub help {
    return
"Build warsow binaries. Usage: !build <os> <arch>.";
}

sub taskfile {
    my ( $os, $arch, $path ) = @_;
    my $file = "$path$os\_$arch";
}

sub status {
    my ( $os, $arch, $run_path, $log_path ) = @_;

    my $url_base = "http://porky.warsow.net/log/";
    my $run_file = taskfile ( $os, $arch, $run_path );
    if (-e $run_file) {
	my $filesize = -s $run_file;
	if ($filesize > 0) {
	    my $pid = `cat $run_file | tr -d '\n'`;
	    my $std_file = taskfile ( $os, $arch, $log_path );
	    $std_file = $std_file . "_$pid";
	    
	    my $std_out = "$std_file\.out";
	    my $std_err = "$std_file\.err";

	    my $out_tail = `tail -1 "$std_out" | tr -d '\n'`;
	    my $err_tail = `tail -1 "$std_err" | tr -d '\n'`;

	    my ($std_out_filename) = fileparse($std_out);
	    my ($std_err_filename) = fileparse($std_err);
	    
	    return "$url_base$std_out_filename : $out_tail\n$url_base$std_err_filename : $err_tail";
	}
    }
}

sub told {
    my ( $self, $mess ) = @_;
    my $body = $mess->{body};

    return 0 unless defined $body;

    my ( $command, $os, $arch ) = split( /\s+/, $body, 3 );
    $command = lc($command);

    return if !$self->authed( $mess->{who} );

    if ( $body =~ /^I\s+love\s+you/i ) {
	return "I love you too";
    }

    my $run_path = "\/home\/porkbot\/build\/run\/";
    my $log_path = "\/home\/porkbot\/build\/log\/";
    if ( $command eq "build" || $command eq "status" || $command eq "rebuild" ) {
	if ( $os eq "sandwich" ) {
	    if ( $command eq "build" ) {
		return "OK! Check your sandwich status in a minute!";
	    }
	    return "Still making!";
	}

	unless ( $os =~ /^(win32|lin)$/ ) {
	    return "Specify the operating system. Possible options: <win32, lin>";
	}
	unless ( $arch =~ /^(x86|x64)$/ ) {
	    return "Specify the target architecture. Possible options: <x86, x64>";
	}

	my $status = status ( $os, $arch, $run_path, $log_path );

	my $run_file = taskfile ( $os, $arch, $run_path );
	my $is_alive = 0;
	if ( -e $run_file ) {
	    system ( "kill -0 `cat $run_file` >/dev/null 2>&1" );
	    if ( $? == 0 ) {
		$is_alive = 1;
	    }
	}

	if ( $is_alive ) {
	    return "Compiling $os $arch...\n$status";
	}

	if ( $command eq "status" ) {
	    if ( -e $run_file ) {
		if ( -s $run_file == 0 ) {
		    return "$os $arch has been scheduled for compilation.";
		}
	    }
	    return "Done.\n$status";
	}

	return "Could not schedule $os $arch" unless open( FH, ">$run_file" );
	close( FH );
	
	if ( $command eq "rebuild" ) {
	    open( FH, ">$run_file\_clean" );
	    close( FH );
	}
	else {
	    unlink( "$run_file\_clean");
	}

	return "Ok, scheduled $os $arch for compilation. Type 'status $os $arch' in a couple of minutes.";
    }

    if ( $command eq "use" ) {
	my ( $command, $what, $how ) = split( /\s+/, $body, 3 );
	
	my $message = "Use what?";

	if ( $what eq "branch" || $what eq "trunk" ) {
	    my $conf_file = "local.inc.sh";
	    my $local_conf = "$run_path$conf_file";
	    
	    my $cur_branch = `grep SOURCE_BRANCH $local_conf | tail -1 | cut -f 2 -d '='`;
	    if ( $cur_branch eq "" ) {
		$cur_branch = "default";
	    }
	    else {
		$cur_branch =~ s/branches\///;
		$cur_branch =~ s/\n//;
		$cur_branch = "'$cur_branch'";
	    }
	    
	    if ( $what eq "branch" ) {
		if ( $how eq "" ) {
		    return "Using $cur_branch branch";
		}
	    }

	    my $local_conf_new = "$local_conf.new";
	    my $local_conf_bak = "$local_conf.bak";
	    my $next_branch = "";

	    `touch "$local_conf"`;
	    `grep -v SOURCE_BRANCH $local_conf > $local_conf_new`;
	    
	    if ( $what eq "trunk" ) {
		$next_branch = "trunk";
		`echo SOURCE_BRANCH="trunk" >> $local_conf_new`;
	    }
	    else {
		if ( $how =~ /[^0-9a-z_\.\/\-]/i ) {
		    return "Invalid branch name $how";
		}

		if ( $how eq "default" ) {
		    $next_branch = "default";
		}
		else {
		    $next_branch = "'$how'";
		    `echo SOURCE_BRANCH="branches\/$how" >> $local_conf_new`;
		}

	    }

	    if ( $next_branch eq $cur_branch ) {
		return "Already on $how";
	    }

	    `rm -f "$local_conf_bak"`;
	    `mv $local_conf $local_conf_bak`;
	    `mv $local_conf_new $local_conf`;
	
	    $message = "Switching to $next_branch, was on $cur_branch";
	}

	return $message;
    }

    if ( $command eq "commit" ) {
	unless ( $os =~ /^(win32|lin)$/ ) {
	    return "Specify the operating system. Possible options: <win32, lin>";
	}
	unless ( $arch =~ /^(x86|x64)$/ ) {
	    return "Specify the target architecture. Possible options: <x86, x64>";
	}

	my $output = `cd /home/porkbot/build/ && ./commit_to_data.sh $os $arch`;
    }
}

1;

__END__

=head1 NAME

Bot::BasicBot::Pluggable::Module::Build - change internal module variables

=head1 VERSION

version 0.1

=head1 SYNOPSIS

Bot modules have variables that they can use to change their behaviour. This
module, when loaded, gives people who are logged in and autneticated the
ability to change these variables from the IRC interface. The variables
that are set are in the object store, and begin "user_", so:

  !set Module foo bar

will set the store key 'user_foo' to 'bar' in the 'Module' module.

=head1 IRC USAGE

=over 4

=item !set <module> <variable> <value>

Sets the variable to value in a given module. Module must be loaded.

=item !unset <module> <variable>

Unsets a variable (deletes it entirely) for the current load of the module.

=item !vars <module>

Lists the variables and their current values in a module.

=back

=head1 AUTHOR

Mario Domgoergen <mdom@cpan.org>

This program is free software; you can redistribute it
and/or modify it under the same terms as Perl itself.