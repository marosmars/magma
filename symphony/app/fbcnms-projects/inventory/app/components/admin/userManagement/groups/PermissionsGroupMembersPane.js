/**
 * Copyright 2004-present Facebook. All Rights Reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @flow strict-local
 * @format
 */

import type {GroupMember} from '../utils/GroupMemberViewer';
import type {PermissionsGroupMembersPaneUserSearchQuery} from './__generated__/PermissionsGroupMembersPaneUserSearchQuery.graphql';
import type {UserPermissionsGroup} from '../utils/UserManagementUtils';

import * as React from 'react';
import Button from '@fbcnms/ui/components/design-system/Button';
import InputAffix from '@fbcnms/ui/components/design-system/Input/InputAffix';
import MembersList from '../utils/MembersList';
import RelayEnvironment from '../../../../common/RelayEnvironment';
import Text from '@fbcnms/ui/components/design-system/Text';
import TextInput from '@fbcnms/ui/components/design-system/Input/TextInput';
import ViewContainer from '@fbcnms/ui/components/design-system/View/ViewContainer';
import classNames from 'classnames';
import fbt from 'fbt';
import symphony from '@fbcnms/ui/theme/symphony';
import {ASSIGNMENT_BUTTON_VIEWS} from '../utils/GroupMemberViewer';
import {
  CloseIcon,
  ProfileIcon,
} from '@fbcnms/ui/components/design-system/Icons/';
import {debounce} from 'lodash';
import {fetchQuery, graphql} from 'relay-runtime';
import {makeStyles} from '@material-ui/styles';
import {useCallback, useMemo, useState} from 'react';
import {userResponse2User} from '../utils/UserManagementUtils';

const userSearchQuery = graphql`
  query PermissionsGroupMembersPaneUserSearchQuery(
    $filters: [UserFilterInput!]!
  ) {
    userSearch(filters: $filters) {
      users {
        id
        authID
        firstName
        lastName
        email
        status
        role
        groups {
          id
          name
        }
        profilePhoto {
          id
          fileName
          storeKey
        }
      }
    }
  }
`;

const useStyles = makeStyles(() => ({
  root: {
    backgroundColor: symphony.palette.white,
    height: '100%',
  },
  header: {
    paddingBottom: '5px',
  },
  title: {
    marginBottom: '16px',
    display: 'flex',
    alignItems: 'center',
  },
  titleIcon: {
    marginRight: '4px',
  },
  userSearch: {
    marginTop: '8px',
  },
  clearSearchIcon: {},
  usersListHeader: {
    display: 'flex',
    justifyContent: 'space-between',
    marginTop: '12px',
    marginBottom: '-3px',
  },
  noMembers: {
    width: '124px',
    paddingTop: '50%',
    alignSelf: 'center',
  },
  noSearchResults: {
    paddingTop: '50%',
    alignSelf: 'center',
    textAlign: 'center',
  },
  clearSearchWrapper: {
    marginTop: '16px',
  },
  clearSearch: {
    ...symphony.typography.subtitle1,
  },
}));

type Props = $ReadOnly<{|
  group: UserPermissionsGroup,
  className?: ?string,
|}>;

const NO_SEARCH_VALUE = '';

export default function PermissionsGroupMembersPane(props: Props) {
  const {group, className} = props;
  const classes = useStyles();
  const [searchIsInProgress, setSearchIsInProgress] = useState(false);
  const [userSearchValue, setUserSearchValue] = useState(NO_SEARCH_VALUE);
  const [usersSearchResult, setUsersSearchResult] = useState<
    Array<GroupMember>,
  >([]);

  const debouncedQueryUsers = useCallback(
    debounce((searchTerm: string) => {
      fetchQuery<PermissionsGroupMembersPaneUserSearchQuery>(
        RelayEnvironment,
        userSearchQuery,
        {
          filters: [
            {
              filterType: 'USER_NAME',
              operator: 'CONTAINS',
              stringValue: searchTerm,
            },
          ],
        },
      )
        .then(response => {
          if (!response?.userSearch) {
            return;
          }
          setUsersSearchResult(
            response.userSearch.users.filter(Boolean).map(userNode => {
              const userData = userResponse2User(userNode);
              return {
                user: userData,
                isMember:
                  userData.groups.find(
                    userGroup => userGroup?.id == group.id,
                  ) != null,
              };
            }),
          );
        })
        .finally(() => setSearchIsInProgress(false));
    }, 200),
    [group],
  );

  const queryUsers = useCallback(
    (searchTerm: string) => {
      debouncedQueryUsers(searchTerm);
      setSearchIsInProgress(true);
    },
    [debouncedQueryUsers],
  );

  const updateSearch = useCallback(
    newSearchValue => {
      setUserSearchValue(newSearchValue);
      if (newSearchValue.trim() == NO_SEARCH_VALUE) {
        setUsersSearchResult([]);
        setSearchIsInProgress(false);
      } else {
        queryUsers(newSearchValue);
      }
    },
    [queryUsers],
  );

  const title = useMemo(
    () => (
      <div className={classes.title}>
        <ProfileIcon className={classes.titleIcon} />
        <fbt desc="">Members</fbt>
      </div>
    ),
    [classes.title, classes.titleIcon],
  );

  const isOnSearchMode = userSearchValue.trim() != NO_SEARCH_VALUE;

  const subtitle = useMemo(
    () => (
      <>
        <Text variant="caption" color="gray" useEllipsis={true}>
          <fbt desc="">Add users to apply policies.</fbt>
        </Text>
        <Text variant="caption" color="gray" useEllipsis={true}>
          <fbt desc="">Users can be members in multiple groups.</fbt>
        </Text>
      </>
    ),
    [],
  );

  const searchBar = useMemo(
    () => (
      <>
        <div className={classes.userSearch}>
          <TextInput
            type="string"
            variant="outlined"
            placeholder={`${fbt('Search users...', '')}`}
            isProcessing={searchIsInProgress}
            fullWidth={true}
            value={userSearchValue}
            onChange={e => updateSearch(e.target.value)}
            suffix={
              isOnSearchMode ? (
                <InputAffix onClick={() => updateSearch(NO_SEARCH_VALUE)}>
                  <CloseIcon className={classes.clearSearchIcon} color="gray" />
                </InputAffix>
              ) : null
            }
          />
        </div>
        {isOnSearchMode ? null : (
          <div className={classes.usersListHeader}>
            {group.members.length > 0 ? (
              <Text variant="subtitle2" useEllipsis={true}>
                <fbt desc="">
                  <fbt:plural count={group.members.length} showCount="yes">
                    Member
                  </fbt:plural>
                </fbt>
              </Text>
            ) : null}
          </div>
        )}
      </>
    ),
    [
      classes.clearSearchIcon,
      classes.userSearch,
      classes.usersListHeader,
      group.members.length,
      isOnSearchMode,
      searchIsInProgress,
      updateSearch,
      userSearchValue,
    ],
  );

  const header = useMemo(
    () => ({
      title,
      subtitle,
      searchBar,
      className: classes.header,
    }),
    [classes.header, searchBar, subtitle, title],
  );

  const memberUsers: $ReadOnlyArray<GroupMember> = useMemo(
    () =>
      isOnSearchMode
        ? usersSearchResult
        : group.memberUsers.map(user => ({
            user: user,
            isMember: true,
          })),
    [isOnSearchMode, usersSearchResult, group.memberUsers],
  );

  return (
    <div className={classNames(classes.root, className)}>
      <ViewContainer header={header}>
        <MembersList
          members={memberUsers}
          group={group}
          assigmentButton={
            isOnSearchMode
              ? ASSIGNMENT_BUTTON_VIEWS.always
              : ASSIGNMENT_BUTTON_VIEWS.onHover
          }
          emptyState={
            isOnSearchMode ? (
              searchIsInProgress ? null : (
                <div className={classes.noSearchResults}>
                  <Text variant="h6" color="gray">
                    <fbt desc="">
                      No users found for '<fbt:param name="given search term">
                        {userSearchValue}
                      </fbt:param>'
                    </fbt>
                  </Text>
                  <div className={classes.clearSearchWrapper}>
                    <Button
                      variant="text"
                      skin="gray"
                      onClick={() => updateSearch(NO_SEARCH_VALUE)}>
                      <span className={classes.clearSearch}>
                        <fbt desc="">Clear Search</fbt>
                      </span>
                    </Button>
                  </div>
                </div>
              )
            ) : (
              <img
                className={classes.noMembers}
                src={'/inventory/static/images/noMembers.png'}
              />
            )
          }
        />
      </ViewContainer>
    </div>
  );
}
