import Base
import ./main.bend as Policy
import ./LAWS.bend as Laws

def Laws.success_resets(attempt):
  {==}

def Laws.challenge_retries(attempt):
  match attempt:
    case Policy.Initial{}:
      {==}
    case Policy.First{}:
      {==}
    case Policy.Second{}:
      {==}
    case Policy.Third{}:
      {==}
    case Policy.Capped{}:
      {==}

def Laws.request_retries(attempt):
  match attempt:
    case Policy.Initial{}:
      {==}
    case Policy.First{}:
      {==}
    case Policy.Second{}:
      {==}
    case Policy.Third{}:
      {==}
    case Policy.Capped{}:
      {==}

def Laws.invalid_url_stops(attempt):
  {==}

def Laws.no_account_stops(attempt):
  {==}

def Laws.payload_failure_stops(attempt):
  {==}

def Laws.signing_failure_stops(attempt):
  {==}

def Laws.authentication_failure_stops(attempt):
  {==}

def Laws.unknown_result_stops(attempt):
  {==}

def Laws.saved_delay_matches(attempt):
  match attempt:
    case Policy.Initial{}:
      {==}
    case Policy.First{}:
      {==}
    case Policy.Second{}:
      {==}
    case Policy.Third{}:
      {==}
    case Policy.Capped{}:
      {==}

def Laws.attempt_number_matches(attempt):
  match attempt:
    case Policy.Initial{}:
      {==}
    case Policy.First{}:
      {==}
    case Policy.Second{}:
      {==}
    case Policy.Third{}:
      {==}
    case Policy.Capped{}:
      {==}
