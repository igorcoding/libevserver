use strict;
use 5.010;
use AnyEvent;
use AnyEvent::Handle;
use AnyEvent::Socket;
use Scalar::Util qw(weaken);
use Time::HiRes 'sleep','time';
use EV;

my $host = '127.0.0.1';
my $port = 9090;

my $parallel = 10;
my $count = 100;

my $msg = "oiwejwoeifjwoeifwoenew";

sub main {
	my $cb = pop;
	
	my $finished = 0;
	my $finish_cb = sub {
		++$finished;
		if ($finished >= $parallel) {
			$cb->();
		}
	};
	
	my $do; $do = sub {
		my $do = $do or return;
		my ($p, $iow, $i) = @_;
		
		my $ccb = sub {
			close $iow->fh;
			$finish_cb->();
		};
		
		$iow->push_write($msg);
		$iow->push_read(
			chunk => length($msg),
			sub {
				
				return $ccb->() unless $i <= $count;
				my $resp = $_[1];
				if ($resp ne $msg) {
					die "resp is invalid: \"$resp\" vs \"$msg\"";
				}
				# warn("[$p] finished $i/$count");
				$do->($p, $iow, $i + 1);
			}
		);
	};
	for my $p (0..$parallel-1) {
		
		tcp_connect $host, $port, sub {
			my ($fh) = @_ or die "connect failed: $!";

			my $iow = new AnyEvent::Handle(
				fh => $fh,
				on_error => sub {
				    my ($hdl, $fatal, $msg) = @_;
				    warn "on_error " . $msg;
					# AE::log error => $msg;
					# $destroy->();
				},
			);
			
			$do->($p, $iow, 1);
		};
	}
}

my $cv = AE::cv();

my $start = time;
main(sub {
	$cv->send;
});
$cv->recv;

my $run = time - $start;
my $per_th = $count / $run;
my $total = $count * $parallel / $run;
warn "[rps] per_thread: $per_th; total: $total";

